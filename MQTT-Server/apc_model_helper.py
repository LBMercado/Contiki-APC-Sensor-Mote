import joblib
import os
from numpy import ndarray


def parse_wind_direction_degrees(deg: float):
    if deg <= 11.25 or deg >= 348.75:
        return 'N'
    elif deg <= 56.25:
        return 'NE'
    elif deg <= 101.25:
        return 'E'
    elif deg <= 146.25:
        return 'SE'
    elif deg <= 191.25:
        return 'S'
    elif deg <= 236.25:
        return 'SW'
    elif deg <= 281.25:
        return 'W'
    else:
        return 'NW'


def load_model(model_name: str):
    loaded_model = {}
    root_folder = 'ml_models'
    dir_name = os.path.join(root_folder, model_name)
    for time_idx in range(1, 13, 1):
        file_name = '{}_t{}_model.sav'.format(model_name, time_idx)
        file_path = os.path.join(dir_name, file_name)
        if os.path.exists(file_path):
            loaded_model['t+{}'.format(time_idx)] = joblib.load(file_path)
            print('Loaded model from file {}'.format(file_name))
        else:
            continue
    if len(loaded_model) != 12:
        print('warning! incomplete or no model loaded.')
    return loaded_model


def lazy_load_model(model_name: str, nth_model: int):
    if nth_model <= 0 or nth_model > 12:
        raise ValueError('nth model exceeds allowed range [1 - 12]')
    loaded_model = None
    root_folder = 'ml_models'
    dir_name = os.path.join(root_folder, model_name)
    file_name = '{}_t{}_model.sav'.format(model_name, nth_model)
    file_path = os.path.join(dir_name, file_name)
    if os.path.exists(file_path):
        loaded_model = joblib.load(file_path)
        print('Loaded model from file {}'.format(file_name))
    else:
        print('warning! no model loaded for {}-th model'.format(nth_model))
    return loaded_model


def reformat_pred_data(preds: ndarray):
    formatted_preds = {
        'PM25 (ug/m3)': preds[0][2], 'CO (PPM)': preds[0][3] / 1000, 'NO2 (PPM)': preds[0][4], 'O3 (PPB)': preds[0][5]
    }
    return formatted_preds
