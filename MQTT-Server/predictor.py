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
