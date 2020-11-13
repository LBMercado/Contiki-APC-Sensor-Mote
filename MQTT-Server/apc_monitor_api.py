from datetime import datetime
from data_access import ApcDataAccess
from apc_model import ApcModel
from to_csv_converter import CsvConverter
from configparser import ConfigParser
from werkzeug.wrappers import Response
from flask import Flask, jsonify
from flask_cors import CORS

api = None
app = Flask(__name__)
app.config['DEBUG'] = True
CORS(app)


@app.route('/', methods=['GET'])
def home():
    return "<h1>RESTful API for APC Monitoring</h1>"


@app.route('/sensor-data/<int:limit>', methods=['GET'])
def get_limit_sensor_data(limit: int):
    return jsonify(api.get_limit_sensor_data(limit))


@app.route('/sensor-data-csv')
def get_sensor_data_csv():
    return api.write_csv_file()


@app.route('/sensor-data', methods=['GET'])
def get_sensor_data():
    return jsonify(api.get_sensor_data())


@app.route('/sensor-datum', methods=['GET'])
def get_sensor_datum():
    return jsonify(api.get_sensor_datum())


@app.route('/mote-info', methods=['GET'])
def get_mote_info():
    return jsonify(api.get_mote_info())


@app.route('/predict/<int:time_step>', methods=['GET'])
def get_prediction(time_step: int):
    return jsonify(api.get_prediction(time_step))


class ApcMonitorApi:
    def __init__(self, db_access: ApcDataAccess, columns_sensor: list, columns_external: list,
                 invalid_values: list, columns_mote_info: list, csv_server_filename: str,
                 predictor_model_name: str, use_api_wind: bool = False, lazy_load_model: bool = False):
        self.db = db_access
        if not self.db.is_connection_active:
            raise ValueError("no connection to database.")
        self.columns_sensor = columns_sensor
        if self.columns_sensor.count == 0:
            raise ValueError("columns_sensor is empty.")
        self.columns_external = columns_external
        self.invalid_values = invalid_values
        self.columns_mote_info = columns_mote_info
        self.tolerance = 10
        self.csv_server_filename = csv_server_filename
        self.model = ApcModel(predictor_model_name, use_api_wind, lazy_load_model)
        self.prediction_in_progress = False

    def write_csv_file(self):
        start_date = None
        end_date = datetime.now()

        converter = CsvConverter(self.db, self.columns_sensor, self.columns_external, self.invalid_values,
                                 self.csv_server_filename, after_date=start_date, before_date=end_date)

        # stream the response as the data is generated
        response = Response(converter.stream_convert(), mimetype='text/csv')
        # add a filename
        response.headers.set("Content-Disposition", "attachment", filename="air-quality-data.csv")
        return response

    def get_sensor_data(self):
        return self.db.get_all_sensor_data_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                                      datetime.now(), None, 10)

    def get_sensor_datum(self):
        return self.db.get_latest_sensor_data_normalized(self.columns_sensor, self.columns_external,
                                                         self.invalid_values,
                                                         datetime.now(), None, 10)

    def get_limit_sensor_data(self, limit: int):
        return self.db.get_limit_sensor_data_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                                        datetime.now(), None, 10, limit)

    def get_mote_info(self):
        return self.db.get_mote_info(self.columns_mote_info)

    def get_prediction(self, time_step: int):
        earliest_apc_data = self.db.get_latest_sensor_data_normalized(self.columns_sensor, self.columns_external,
                                                                      self.invalid_values, datetime.now(), None, 10)
        earliest_date = earliest_apc_data['date']
        pred_data = self.db.get_limit_predictions(time_step, earliest_date)

        if len(pred_data) < time_step and not self.prediction_in_progress:
            self.prediction_in_progress = True
            apc_data = self.db.get_limit_sensor_data_normalized(self.columns_sensor, self.columns_external,
                                                                self.invalid_values, datetime.now(), None, 10, 13)
            apc_data.reverse()
            pred_data = self.model.predict(apc_data, time_step)

            delete_count = self.db.delete_documents({'date': {'$gte': earliest_date}}, self.db.pred_data_coll_name)
            print('deleted {} old forecasts'.format(delete_count))
            for preds in pred_data:
                self.db.insert_document(preds, self.db.pred_data_coll_name)
                del preds['_id']

            self.prediction_in_progress = False
        return pred_data


def init():
    global api

    config = ConfigParser()
    config.read('config.ini')

    # db config
    db_name = config['mongodb']['db_name']
    db_address = config['mongodb']['db_address']
    db_port = int(config['mongodb']['db_port'])
    db_mote_data_collection = config['mongodb']['db_motedata_collection']
    db_pred_data_collection = config['mongodb']['db_prediction_collection']

    # csv file creation config
    columns_sensor = config['csv_api']['columns_sensor'].split(',')
    columns_sensor = [x.strip() for x in columns_sensor]
    columns_external = config['csv_api']['columns_external'].split(',')
    columns_external = [x.strip() for x in columns_external]
    columns_mote_info = config['csv_api']['columns_mote_info'].split(',')
    columns_mote_info = [x.strip() for x in columns_mote_info]
    invalid_values = config['apc_model']['invalid_values'].split(',')
    invalid_values = [x.strip() for x in invalid_values]
    for i in range(len(invalid_values)):
        if _is_number(invalid_values[i]):
            invalid_values[i] = int(invalid_values[i])
    csv_server_filename = config['apc_model']['server_file']

    # apc model config
    model_name = config['apc_model']['predictor_model_name']
    use_api_wind = config['apc_model']['use_api_wind'].strip() == '1'
    lazy_load_model = config['apc_model']['lazy_load'].strip() == '1'

    db_access = ApcDataAccess(db_address, db_port, db_name, db_mote_data_collection, db_pred_data_collection)
    api = ApcMonitorApi(db_access, columns_sensor, columns_external, invalid_values, columns_mote_info,
                        csv_server_filename, model_name, use_api_wind, lazy_load_model)

    print('initialization complete!')


# helper function for invalid_values
def _is_number(num_string: str):
    try:
        f = float(num_string)
        return True
    except ValueError:
        return False


if __name__ == "__main__":
    init()
    app.run(host='0.0.0.0')
