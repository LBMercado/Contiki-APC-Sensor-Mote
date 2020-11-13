from apc_label_encoder import ApcLabelEncoder
from apc_model_helper import parse_wind_direction_degrees, load_model, lazy_load_model, reformat_pred_data
from predictor import SlidingPredictor, LazySlidingPredictor
from typing import Dict, List
from datetime import timedelta
import numpy as np


class ApcModel:
    def __init__(self, model_name: str = None, use_api_wind: bool = False, lazy_load: bool = False):
        self.predictor_model = None
        self.use_api_wind = use_api_wind
        self.lazy_load = lazy_load
        self.model_name = model_name
        if model_name:
            self.build_model(model_name)
        self.label_encoder = ApcLabelEncoder()

    def build_model(self, model_name: str):
        if not self.lazy_load:
            model_list = load_model(model_name)
            self.predictor_model = SlidingPredictor('recurrent', model_list)
        else:
            self.predictor_model = LazySlidingPredictor('recurrent')

    '''
        apc data must be ascending time order! 
    '''

    def predict(self, apc_data_window: List[Dict], time_step: int):
        if self.predictor_model is None:
            raise ValueError('no model defined')
        if not self.lazy_load:
            return self._normal_predict(apc_data_window, time_step)
        else:
            return self._lazy_predict(apc_data_window, time_step)

    def _normal_predict(self, apc_data_window: List[Dict], time_step: int):
        sliding_window = np.empty
        present_date = apc_data_window[len(apc_data_window) - 1]['date']
        for idx, apc_data in enumerate(apc_data_window):
            window = self._reformat_apc_data(apc_data)
            if idx == len(apc_data_window) - 1:
                present_date = apc_data['date']
            if idx == 0:
                sliding_window = window
            else:
                sliding_window = np.hstack((sliding_window, window))

        preds_list = self.predictor_model.predict(sliding_window, time_step)
        for idx in range(len(preds_list)):
            preds_list[idx] = reformat_pred_data(preds_list[idx])
            preds_list[idx]['date'] = timedelta(hours=idx + 1) + present_date
        return preds_list

    def _lazy_predict(self, apc_data_window: List[Dict], time_step: int):
        sliding_window = np.empty
        present_date = apc_data_window[len(apc_data_window) - 1]['date']
        for idx, apc_data in enumerate(apc_data_window):
            window = self._reformat_apc_data(apc_data)
            if idx == 0:
                sliding_window = window
            else:
                sliding_window = np.hstack((sliding_window, window))

        preds_list = list()
        for cur_time_step in range(1, time_step + 1):
            model = lazy_load_model(self.model_name, cur_time_step)
            sliding_window, preds = self.predictor_model.lazy_predict(sliding_window, model)
            formatted_preds = reformat_pred_data(preds)
            formatted_preds['date'] = timedelta(hours=cur_time_step) + present_date
            preds_list.append(formatted_preds)

            del model

        return preds_list

    def _reformat_apc_data(self, apc_data: Dict) -> np.ndarray:
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

        # reformat NO2 ppm to ppb
        apc_data['NO2 (PPM)'] *= 1000

        return np.array(list(apc_data.values()))
