from pymongo import MongoClient
from typing import List


class DataAccess:
    def __init__(self, address, port: int, collection_name: str):
        try:
            if not address or not port or port < 1:
                raise ValueError("Invalid address or port specified.")
            self.address = address
            self.port = port
            self.collection_name = collection_name
            self.is_connection_active = False
            self.cur_db = None
            self.client = MongoClient(address, port)
        except ValueError as err:
            print(err)

    def connect_db(self, db_name: str):
        if not db_name:
            print("Invalid database name specified.")
            return
        self.cur_db = self.client[db_name]
        self.is_connection_active = True

    def disconnect_db(self):
        self.cur_db = None
        self.is_connection_active = False

    def insert_document(self, document: dict):
        if not self.is_connection_active or self.cur_db is None:
            print("Failed to insert document: No active database connection.")
            return
        if not self.collection_name or not document:
            print("Failed to insert document: collection_name or document is null.")
            return
        collection = self.cur_db[self.collection_name]
        result = collection.insert_one(document)
        print("Inserted in collection \'{}\' document with id: {}".format(self.collection_name, result.inserted_id))

    def get_documents(self, criteria: dict):
        documents = []
        for document in self.cur_db[self.collection_name].find(criteria):
            documents.append(document)
        return documents
