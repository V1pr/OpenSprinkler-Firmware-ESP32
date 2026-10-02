#include "aggregate_sensor.h"
#include "../OpenSprinkler.h"

extern OpenSprinkler os;

AggregateSensor::AggregateSensor(uint32_t interval, float min, float max, const char* name, SensorUnit unit, uint8_t flag, sensor_memory_t* sensors, aggregate_children_t* children, uint8_t children_count, AggregateAction action) :
	Sensor(interval, min, max, name, unit, flag),
	action(action),
	sensors(sensors) {
	for (size_t i = 0; i < AGGREGATE_SENSOR_CHILDREN_COUNT; i++) {
		if (i < children_count) {
			this->children[i] = children[i];
		} else {
			this->children[i] = aggregate_children_t{ AGGREGATE_CHILD_DEFAULT_SCALE, AGGREGATE_CHILD_DEFAULT_OFFSET, SENSOR_UUID_NONE };
		}
	}
}

void AggregateSensor::emit_extra_json(BufferFiller* bfill) {
	bfill->emit_p(PSTR("{\"action\":$D,\"children\":["), this->action);
	for (size_t i = 0; i < AGGREGATE_SENSOR_CHILDREN_COUNT; i++) {
		if (i) bfill->emit_p(PSTR(","));
		aggregate_children_t* child = &this->children[i];
		bfill->emit_p(PSTR("{\"uuid\":$D,\"scale\":$E,\"offset\":$E}"), child->uuid, child->scale, child->offset);
	}
	bfill->emit_p(PSTR("]}"));
}

void AggregateSensor::emit_description_json(BufferFiller* bfill) {
	bfill->emit_p(PSTR(
		"{\"n\":\"Aggregate Sensor\","
		"\"as\":["
			"{\"n\":\"Children\","
			 "\"a\":\"children\","
			 "\"t\":\"array::" SENSOR_DEFAULT_STR(AGGREGATE_SENSOR_CHILDREN_COUNT) "\","
			 "\"e\":["
				"{\"n\":\"Sensor UUID\",\"a\":\"uuid\",\"t\":\"sensor\",\"d\":\"" SENSOR_DEFAULT_STR(SENSOR_UUID_NONE) "\",\"indicator\":true},"
				"{\"n\":\"Scale\",\"a\":\"scale\",\"t\":\"float\",\"d\":\"" SENSOR_DEFAULT_STR(AGGREGATE_CHILD_DEFAULT_SCALE) "\"},"
				"{\"n\":\"Offset\",\"a\":\"offset\",\"t\":\"float\",\"d\":\"" SENSOR_DEFAULT_STR(AGGREGATE_CHILD_DEFAULT_OFFSET) "\"}"
			"]},"
			"{\"n\":\"Action\","
			 "\"a\":\"action\","
			 "\"t\":\"enum::AggregateAction\","
			 "\"d\":\"" SENSOR_DEFAULT_STR(AGGREGATE_DEFAULT_ACTION) "\"}"
		"]}"
	));
}


float AggregateSensor::_get_raw_value() {
	float values[AGGREGATE_SENSOR_CHILDREN_COUNT];
	uint8_t count = 0;

	for (size_t i = 0; i < AGGREGATE_SENSOR_CHILDREN_COUNT; i++) {
		if (this->children[i].uuid == SENSOR_UUID_NONE) continue;
		uint8_t idx = Sensor::find_index(this->children[i].uuid);
		if (idx >= OpenSprinkler::nsensors || !sensors[idx].interval) continue;
		if (!(sensors[idx].status & SENSOR_STATUS_VALID)) continue;

		values[count++] = sensors[idx].value * this->children[i].scale + this->children[i].offset;
	}

	if (count == 0) return NAN;

	switch (this->action) {
	case AggregateAction::Min: {
		float result = values[0];
		for (uint8_t i = 1; i < count; i++) if (values[i] < result) result = values[i];
		return result;
	}
	case AggregateAction::Max: {
		float result = values[0];
		for (uint8_t i = 1; i < count; i++) if (values[i] > result) result = values[i];
		return result;
	}
	case AggregateAction::Average: {
		float sum = 0;
		for (uint8_t i = 0; i < count; i++) sum += values[i];
		return sum / count;
	}
	case AggregateAction::Sum: {
		float sum = 0;
		for (uint8_t i = 0; i < count; i++) sum += values[i];
		return sum;
	}
	case AggregateAction::Median: {
		// Insertion sort (up to AGGREGATE_SENSOR_CHILDREN_COUNT elements)
		for (uint8_t i = 1; i < count; i++) {
			float key = values[i];
			int8_t j = i - 1;
			while (j >= 0 && values[j] > key) { values[j + 1] = values[j]; j--; }
			values[j + 1] = key;
		}
		if (count % 2 == 1) return values[count / 2];
		return (values[count / 2 - 1] + values[count / 2]) / 2.0f;
	}
	case AggregateAction::Range: {
		float lo = values[0], hi = values[0];
		for (uint8_t i = 1; i < count; i++) {
			if (values[i] < lo) lo = values[i];
			if (values[i] > hi) hi = values[i];
		}
		return hi - lo;
	}
	default:
		return NAN;
	}
}

uint32_t AggregateSensor::_serialize_internal(char* buf) {
	uint32_t i = 0;
	for (size_t j = 0; j < AGGREGATE_SENSOR_CHILDREN_COUNT; j++) {
		i += write_buf(buf + i, this->children[j].scale);
		i += write_buf(buf + i, this->children[j].offset);
		i += write_buf(buf + i, this->children[j].uuid);
	}
	buf[i++] = static_cast<uint8_t>(this->action);
	return i;
}

AggregateSensor::AggregateSensor(sensor_memory_t* sensors, char* buf, uint32_t len) {
	uint8_t subclass_len = 0;
	uint32_t i = Sensor::_deserialize(buf, len, &subclass_len);
	uint32_t end = i + subclass_len;

	for (size_t j = 0; j < AGGREGATE_SENSOR_CHILDREN_COUNT; j++) {
		this->children[j] = aggregate_children_t{ AGGREGATE_CHILD_DEFAULT_SCALE, AGGREGATE_CHILD_DEFAULT_OFFSET, SENSOR_UUID_NONE };
		read_buf(buf, &i, end, this->children[j].scale);
		read_buf(buf, &i, end, this->children[j].offset);
		read_buf(buf, &i, end, this->children[j].uuid);
	}
	this->action = static_cast<AggregateAction>(AGGREGATE_DEFAULT_ACTION);
	if (i + 1 <= end) this->action = static_cast<AggregateAction>(buf[i]);
	this->sensors = sensors;
}
