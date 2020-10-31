from configparser import ConfigParser
from apc_label_encoder import ApcLabelEncoder
from apc_model_helper import parse_wind_direction_degrees, load_model, reformat_pred_data
from predictor import Predictor
from typing import Dict
from datetime import timedelta
import pandas as pd


class ApcModel:
    def __init__(self, model_name: str = None):
        self.predictor_model = None
        if model_name:
            self.build_model(model_name)
        config = ConfigParser()
        config.read('config.ini')
        try:
            self.use_api_wind = int(config['apc_model']['use_api_wind']) != 0
        except ValueError:
            # use default setting
            self.use_api_wind = True
        self.label_encoder = ApcLabelEncoder()

    def build_model(self, model_name: str):
        model_list = load_model(model_name)
        self.predictor_model = Predictor('recurrent', model_list)

    def predict(self, current_apc_data: Dict, time_step: int):
        cur_date = current_apc_data['date']
        df = self._reformat_apc_data(current_apc_data)
        # pass the df to the model and return its result
        if self.predictor_model is None:
            raise ValueError('no model defined')
        preds = self.predictor_model.predict(df, time_step)
        formatted_preds = reformat_pred_data(preds)
        formatted_preds['date'] = timedelta(hours=time_step) + cur_date
        return formatted_preds

    def _reformat_apc_data(self, apc_data: Dict) -> pd.DataFrame:
        # reformat wind speed and direction of choice
        if 'api_wind' in apc_data:
            if self.use_api_wind and apc_data['api_wind']:
                apc_data['Wind Direction'] = parse_wind_direction_degrees(apc_data['api_wind']['deg'])
                apc_data['Wind Speed (m/s)'] = apc_data['api_wind']['speed']
            elif self.use_api_wind and not apc_data['api_wind']:
                apc_data['Wind Direction'] = 'N'
                apc_data['Wind Speed (m/s)'] = -1
            del apc_data['api_wind']
        del apc_data['date']

        # reformat labels to numerical value
        if apc_data['Wind Direction']:
            apc_data['Wind Direction'] = self.label_encoder.encode_wind_direction(apc_data['Wind Direction'])[0]
        else:
            # TODO: should raise an error
            apc_data['Wind Direction'] = self.label_encoder.encode_wind_direction('N')[0]
        if apc_data['api_weather']:
            apc_data['api_weather'] = self.label_encoder.encode_weather_condition(apc_data['api_weather'])[0]
        else:
            # TODO: should raise an error
            apc_data['api_weather'] = self.label_encoder.encode_weather_condition('Clouds')[0]

        return pd.DataFrame([apc_data])
