from data_access import DataAccess
from sensor_aggregator import SensorAggregator
import datetime


def db_get_all_sensor_data_normalized(columns_sensor: list, columns_external: list, invalid_values: list,
                                      before_date: datetime, after_date, tolerance: float,
                                      db: DataAccess):
    filter_column = {'$or': []}
    filter_column['$or'] \
        .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

    # make sure at least one valid field exists in document
    #   sensor data
    for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
        for key in or_clause.keys():
            or_clause[key] = {'$exists': True}

    #   external data
    for column in columns_external:
        filter_column['$or'].append({column: {'$exists': True}})
        if column == 'date':
            # check if dates are provided and specify query restriction, otherwise ignore
            #       get all data later than the after_date specified
            if after_date is not None and before_date is None:
                filter_column['date'] = {'$gte': after_date}
            #       get data between the two dates specified
            elif after_date is not None and before_date is not None:
                filter_column['date'] = {'$gte': after_date, '$lte': before_date}
            #       get all data prior to the before_date specified
            elif after_date is None and before_date is not None:
                filter_column['date'] = {'$lte': before_date}

    print('filter set: ' + str(filter_column))

    documents = db.get_documents(filter_column)
    print('found {} data from collection \'{}\''.format(len(documents), db.collection_name))

    # remove unneeded key pairs
    for document in documents:
        for key in list(document.keys()):
            if key == 'collectors_data':
                for collector in document['collectors_data']:
                    for inner_key, inner_value in list(collector.items()):
                        if inner_key not in columns_sensor \
                                or inner_value in invalid_values:
                            del collector[inner_key]

            elif key not in columns_external:
                del document[key]

    # normalize sensor data values
    for i in range(len(documents)):
        aggregator = SensorAggregator(documents[i]['collectors_data'], tolerance)
        aggregator.aggregate()
        avg_sensor_data = aggregator.average_sensor_data

        # add in the external fields
        for key in documents[i]:
            if key in columns_external:
                avg_sensor_data[key] = documents[i][key]
        documents[i] = avg_sensor_data

        # pad empty fields
        column_list = [*columns_sensor, *columns_external]
        for column in column_list:
            if column not in documents[i].keys():
                documents[i][column] = ''

        # stringify array fields
        for key, val in documents[i].items():
            if isinstance(val, list):
                new_val = str(val).replace(',', '&')
                # remove non-alphanumeric characters
                new_val = ''.join(char for char in new_val if char.isalnum() or char == '&')
                documents[i][key] = new_val

    return documents


def db_limit_get_sensor_data_normalized(columns_sensor: list, columns_external: list, invalid_values: list,
                                        before_date: datetime, after_date, tolerance: float, limit: int,
                                        db: DataAccess):
    filter_column = {'$or': []}
    filter_column['$or'] \
        .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

    # make sure at least one valid field exists in document
    #   sensor data
    for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
        for key in or_clause.keys():
            or_clause[key] = {'$exists': True}

    #   external data
    for column in columns_external:
        filter_column['$or'].append({column: {'$exists': True}})
        if column == 'date':
            # check if dates are provided and specify query restriction, otherwise ignore
            #       get all data later than the after_date specified
            if after_date is not None and before_date is None:
                filter_column['date'] = {'$gte': after_date}
            #       get data between the two dates specified
            elif after_date is not None and before_date is not None:
                filter_column['date'] = {'$gte': after_date, '$lte': before_date}
            #       get all data prior to the before_date specified
            elif after_date is None and before_date is not None:
                filter_column['date'] = {'$lte': before_date}

    print('filter set: ' + str(filter_column))

    documents = db.get_first_documents(filter_column, limit)
    print('found {} data from collection \'{}\''.format(len(documents), db.collection_name))

    # remove unneeded key pairs
    for document in documents:
        for key in list(document.keys()):
            if key == 'collectors_data':
                for collector in document['collectors_data']:
                    for inner_key, inner_value in list(collector.items()):
                        if inner_key not in columns_sensor \
                                or inner_value in invalid_values:
                            del collector[inner_key]

            elif key not in columns_external:
                del document[key]

    # normalize sensor data values
    for i in range(len(documents)):
        aggregator = SensorAggregator(documents[i]['collectors_data'], tolerance)
        aggregator.aggregate()
        avg_sensor_data = aggregator.average_sensor_data

        # add in the external fields
        for key in documents[i]:
            if key in columns_external:
                avg_sensor_data[key] = documents[i][key]
        documents[i] = avg_sensor_data

        # pad empty fields
        column_list = [*columns_sensor, *columns_external]
        for column in column_list:
            if column not in documents[i].keys():
                documents[i][column] = ''

        # stringify array fields
        for key, val in documents[i].items():
            if isinstance(val, list):
                new_val = str(val).replace(',', '&')
                # remove non-alphanumeric characters
                new_val = ''.join(char for char in new_val if char.isalnum() or char == '&')
                documents[i][key] = new_val

    return documents


def db_get_latest_sensor_data_normalized(columns_sensor: list, columns_external: list, invalid_values: list,
                                         before_date: datetime, after_date, tolerance: float,
                                         db: DataAccess):
    filter_column = {'$or': []}
    filter_column['$or'] \
        .append({'collectors_data': {'$elemMatch': {'$or': [{key: {}} for key in columns_sensor]}}})

    # make sure at least one valid field exists in document
    #   sensor data
    for or_clause in filter_column['$or'][0]['collectors_data']['$elemMatch']['$or']:
        for key in or_clause.keys():
            or_clause[key] = {'$exists': True}

    #   external data
    for column in columns_external:
        filter_column['$or'].append({column: {'$exists': True}})
        if column == 'date':
            # check if dates are provided and specify query restriction, otherwise ignore
            #       get all data later than the after_date specified
            if after_date is not None and before_date is None:
                filter_column['date'] = {'$gte': after_date}
            #       get data between the two dates specified
            elif after_date is not None and before_date is not None:
                filter_column['date'] = {'$gte': after_date, '$lte': before_date}
            #       get all data prior to the before_date specified
            elif after_date is None and before_date is not None:
                filter_column['date'] = {'$lte': before_date}

    print('filter set: ' + str(filter_column))

    document = db.get_first_document(filter_column)

    if document is None or document == {}:
        return None

    # remove unneeded key pairs
    for key in list(document.keys()):
        if key == 'collectors_data':
            for collector in document['collectors_data']:
                for inner_key, inner_value in list(collector.items()):
                    if inner_key not in columns_sensor \
                            or inner_value in invalid_values:
                        del collector[inner_key]

        elif key not in columns_external:
            del document[key]

    # normalize sensor data values
    aggregator = SensorAggregator(document['collectors_data'], tolerance)
    aggregator.aggregate()
    avg_sensor_data = aggregator.average_sensor_data

    # add in the external fields
    for key in document:
        if key in columns_external:
            avg_sensor_data[key] = document[key]
    document = avg_sensor_data

    # pad empty fields
    column_list = [*columns_sensor, *columns_external]
    for column in column_list:
        if column not in document.keys():
            document[column] = ''

    # stringify array fields
    for key, val in document.items():
        if isinstance(val, list):
            new_val = str(val).replace(',', '&')
            # remove non-alphanumeric characters
            new_val = ''.join(char for char in new_val if char.isalnum() or char == '&')
            document[key] = new_val

    return document


def db_get_mote_info(columns_include: list, db: DataAccess):
    filter_column = {'$or': []}
    filter_column['$or'] \
        .append({'collectors_info': {'$elemMatch': {'$or': [{key: {}} for key in columns_include]}}})

    # make sure at least one valid field exists in document
    for or_clause in filter_column['$or'][0]['collectors_info']['$elemMatch']['$or']:
        for key in or_clause.keys():
            or_clause[key] = {'$exists': True}

    # get latest document
    filter_column['date'] = {'$lte': datetime.datetime.now()}

    print('filter set: ' + str(filter_column))

    document = db.get_first_document(filter_column)

    # remove unneeded key pairs
    for key in list(document.keys()):
        if key == 'collectors_info':
            for collector in document['collectors_info']:
                for inner_key, inner_value in list(collector.items()):
                    if inner_key not in columns_include:
                        del collector[inner_key]

        else:
            del document[key]

    document = document['collectors_info']
    return document
