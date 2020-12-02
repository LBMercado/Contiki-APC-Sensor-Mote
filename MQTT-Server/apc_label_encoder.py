from sklearn.preprocessing import LabelEncoder
from datetime import datetime

from typing import List


class ApcLabelEncoder:
    def __init__(self):
        self.wind_direction_le = LabelEncoder()
        self.wind_direction_le.fit(['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'])
        self.weather_condition_le = LabelEncoder()
        self.weather_condition_le.fit((['CLOUDS', 'RAIN', 'CLEAR', 'RAIN&THUNDERSTORM', 'THUNDERSTORM', 'MIST']))

    def encode_wind_direction(self, wind_direction: str):
        wind_direction = wind_direction.strip().upper()
        return self.wind_direction_le.transform([wind_direction])

    def encode_weather_condition(self, weather_condition: str):
        weather_condition = weather_condition.strip().upper()
        return self.weather_condition_le.transform([weather_condition])
