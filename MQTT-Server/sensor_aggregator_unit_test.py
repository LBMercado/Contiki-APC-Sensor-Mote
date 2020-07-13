import unittest
from sensor_aggregator import SensorAggregator


class SensorAggregatorTestCase(unittest.TestCase):
    def test_should_aggregate_with_correct_inputs(self):
        sensor_data1 = {
            "Temperature (°C)": 35.0,
            "Humidity (H)": 66.7,
            "PM25 (ug/m3)": 200,
            "NO2 (PPM)": 250,
            "CO (PPM)": 125,
            "O3 (PPB)": 100,
            "Wind Speed (m/s)": 1.7,
            "Wind Direction": "NE"
        }
        sensor_data2 = {
            "Temperature (°C)": 35.1,
            "Humidity (H)": 66.3,
            "PM25 (ug/m3)": 220,
            "NO2 (PPM)": 299,
            "CO (PPM)": 127,
            "O3 (PPB)": 125,
            "Wind Speed (m/s)": 1.5,
            "Wind Direction": "NE"
        }

        sensor_data = [sensor_data1, sensor_data2]
        agg = SensorAggregator(sensor_data, 50)
        agg.aggregate()

        avg_sensor_data = agg.average_sensor_data

        for key in sensor_data1:
            if not isinstance(avg_sensor_data[key], str):
                self.assertEqual(avg_sensor_data[key], (sensor_data1[key] + sensor_data2[key]) / 2)
            else:
                self.assertEqual(avg_sensor_data[key], sensor_data1[key])

    def test_should_aggregate_with_some_incorrect_inputs(self):
        sensor_data1 = {
            "Temperature (°C)": '',
            "Humidity (H)": '',
            "PM25 (ug/m3)": 200,
            "NO2 (PPM)": 250,
            "CO (PPM)": 125,
            "O3 (PPB)": 100,
            "Wind Speed (m/s)": 1.7,
            "Wind Direction": "NE"
        }
        sensor_data2 = {
            "Temperature (°C)": 35.1,
            "Humidity (H)": 66.3,
            "PM25 (ug/m3)": 220,
            "NO2 (PPM)": 299,
            "CO (PPM)": 127,
            "O3 (PPB)": 125,
            "Wind Speed (m/s)": 1.5,
            "Wind Direction": ''
        }

        sensor_data = [sensor_data1, sensor_data2]
        agg = SensorAggregator(sensor_data, 50)
        agg.aggregate()

        avg_sensor_data = agg.average_sensor_data

        for key in sensor_data1:
            if key == "Temperature (°C)" or key == "Humidity (H)":
                self.assertEqual(avg_sensor_data[key], sensor_data2[key])
            elif not isinstance(avg_sensor_data[key], str):
                self.assertEqual(avg_sensor_data[key], (sensor_data1[key] + sensor_data2[key]) / 2)
            else:
                self.assertEqual(avg_sensor_data[key], sensor_data1[key])


if __name__ == '__main__':
    unittest.main()
