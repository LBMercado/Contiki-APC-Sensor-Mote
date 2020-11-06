import pandas as pd
import numpy as np


class Predictor:
    def __init__(self, model_type, predictors=None):
        if model_type == 'direct' or model_type == 'recurrent':
            self.model_type = model_type
        else:
            self.model_type = 'recurrent'
        self.model_list = list()
        self.predictors = predictors
        if predictors:
            self._build_model()

    def predict(self, inputs, time_step) -> list:
        predictions = list()

        if len(self.model_list) == 0:
            raise ValueError('No model initialized.')
        if self.model_type == 'direct':
            if time_step > len(self.model_list):
                time_step = len(self.model_list)
            elif time_step <= 0:
                time_step = 1
            for cur_step in range(time_step):
                predictions.append(self.model_list[cur_step].predict(inputs))
            return predictions
        elif self.model_type == 'recurrent':
            if time_step > len(self.model_list):
                time_step = len(self.model_list)
            elif time_step <= 0:
                time_step = 1
            preds = None
            for idx in range(time_step):
                if idx == 0:
                    preds = self.model_list[0].predict(inputs)
                else:
                    preds = self.model_list[idx].predict(preds)
                predictions.append(preds)
            return predictions
        raise ValueError('Invalid model type specified = {}.'.format(self.model_type))

    def _build_model(self):
        self.model_list = []
        for model in self.predictors.values():
            self.model_list.append(model)


class SlidingPredictor(Predictor):
    def predict(self, inputs, time_step) -> list:
        input_np_arr = None
        predictions = list()
        if isinstance(inputs, pd.DataFrame):
            input_np_arr = inputs.to_numpy()
        elif isinstance(np.array, inputs):
            input_np_arr = inputs
        elif isinstance(list, (pd.Series, np.ndarray)):
            input_np_arr = np.array(inputs)
        else:
            raise ValueError('incorrect datatype for value passed: {}, must be a list, numpy array, or pandas '
                             'data structure'.format(type(inputs)))

        if len(self.model_list) == 0:
            raise ValueError('model is not initialized')
        if self.model_type == 'direct':
            if time_step > len(self.model_list):
                time_step = len(self.model_list)
            elif time_step <= 0:
                time_step = 1

            for cur_step in range(time_step):
                predictions.append(self.model_list[cur_step].predict(input_np_arr))
            return predictions

        elif self.model_type == 'recurrent':
            if time_step > len(self.model_list):
                time_step = len(self.model_list)
            elif time_step <= 0:
                time_step = 1
            # perform a sliding window approach to the input
            preds = None
            for idx in range(time_step):
                if idx == 0:
                    preds = self.model_list[idx].predict(input_np_arr)
                else:
                    # remove the earliest step (first 9 columns)
                    input_arr = np.delete(input_np_arr, slice(0, 9), axis=1)

                    # pad the predicted results in the last step (last 9 columns)
                    input_arr = np.append(input_arr, preds, axis=1)

                    # predict the next step
                    preds = self.model_list[idx].predict(input_arr)
                predictions.append(preds)
            return predictions
        else:
            raise ValueError('unexpected model type: {}'.format(self.model_type))


class LazySlidingPredictor(SlidingPredictor):
    def lazy_predict(self, inputs, predictor):
        input_np_arr = None
        preds = None
        if isinstance(inputs, pd.DataFrame):
            input_np_arr = inputs.to_numpy()
        elif isinstance(inputs, (np.ndarray, np.generic)):
            input_np_arr = inputs
        elif isinstance(inputs, (list, pd.Series, np.ndarray)):
            input_np_arr = np.array(inputs)
        else:
            raise ValueError('incorrect datatype for value passed: {}, must be a list, numpy array, or pandas '
                             'data structure'.format(type(inputs)))
        # check if single sample
        if len(input_np_arr.shape) == 1:
            input_np_arr = input_np_arr.reshape(1, -1)

        preds = predictor.predict(input_np_arr)

        # remove the earliest step (first 9 columns)
        input_arr = np.delete(input_np_arr, slice(0, 9), axis=1)

        # pad the predicted results in the last step (last 9 columns)
        input_arr = np.append(input_arr, preds, axis=1)

        return input_arr, preds
