from db_helper import db_get_all_sensor_data_normalized, db_get_mote_info, db_get_latest_sensor_data_normalized, \
    db_limit_get_sensor_data_normalized
from data_access import DataAccess
from to_csv_converter import CsvConverter
from configparser import ConfigParser
from werkzeug.wrappers import Response
import datetime
import os
from flask import Flask
from flask import jsonify, send_file

app = Flask(__name__)
app.config['DEBUG'] = True

api = None

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


class ApcMonitorApi:
    def __init__(self, db_access: DataAccess, columns_sensor: list, columns_external: list,
                 invalid_values: list, columns_mote_info: list,  csv_server_filename: str):
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

    def write_csv_file(self):
        start_date = None
        end_date = datetime.datetime.now()

        converter = CsvConverter(self.db, self.columns_sensor, self.columns_external, self.invalid_values,
                                 self.csv_server_filename, after_date=start_date, before_date=end_date)

        # stream the response as the data is generated
        response = Response(converter.stream_convert(), mimetype='text/csv')
        # add a filename
        response.headers.set("Content-Disposition", "attachment", filename="air-quality-data.csv")
        return response

    def get_sensor_data(self):
        return db_get_all_sensor_data_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                                 datetime.datetime.now(), None, 10, self.db)

    def get_sensor_datum(self):
        return db_get_latest_sensor_data_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                                    datetime.datetime.now(), None, 10, self.db)

    def get_limit_sensor_data(self, limit: int):
        return db_limit_get_sensor_data_normalized(self.columns_sensor, self.columns_external, self.invalid_values,
                                                   datetime.datetime.now(), None, 10, limit, self.db)

    def get_mote_info(self):
        return db_get_mote_info(self.columns_mote_info, self.db)


def init():
    global api

    config = ConfigParser()
    config.read('config.ini')

    db_name = config['mongodb']['db_name']
    db_address = config['mongodb']['db_address']
    db_port = int(config['mongodb']['db_port'])
    db_collection = config['mongodb']['db_collection']
    columns_sensor = config['csv_api']['columns_sensor'].split(',')
    columns_sensor = [x.strip() for x in columns_sensor]
    columns_external = config['csv_api']['columns_external'].split(',')
    columns_external = [x.strip() for x in columns_external]
    columns_mote_info = config['csv_api']['columns_mote_info'].split(',')
    columns_mote_info = [x.strip() for x in columns_mote_info]
    invalid_values = config['csv_api']['invalid_values'].split(',')
    invalid_values = [x.strip() for x in invalid_values]
    for i in range(len(invalid_values)):
        if _is_number(invalid_values[i]):
            invalid_values[i] = int(invalid_values[i])
    csv_server_filename = config['csv_api']['server_file']

    db_access = DataAccess(db_address, db_port, db_name, db_collection)
    api = ApcMonitorApi(db_access, columns_sensor, columns_external, invalid_values, columns_mote_info,
                        csv_server_filename)


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
