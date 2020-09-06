import configparser
import requests
import json
import sys
 
def get_weather_with_loc(api_key, location):
    url = "https://api.openweathermap.org/data/2.5/weather?q={}&units=metric&appid={}".format(location, api_key)
    r = requests.get(url)
    if r.status_code == 200:
        return r.json()
    elif r.status_code == 404:
        print("Weather API: ERROR - API url does not exist.")
        return None
    elif r.status_code == 403:
        print("Weather API: ERROR - Forbidden.")
        return None
    elif r.status_code == 401:
        print("Weather API: ERROR - Unauthorized.")
        return None
    elif r.status_code == 429:
        print("Weather API: ERROR - Exceeded maximum API call count.")
        return None
    else:
        print("Weather API: ERROR - Unknown error.")
        return None

def get_weather_with_id(api_key, loc_id):
    url = "https://api.openweathermap.org/data/2.5/weather?id={}&units=metric&appid={}".format(loc_id, api_key)
    r = requests.get(url)
    if r.status_code == 200:
        return r.json()
    elif r.status_code == 404:
        print("Weather API: ERROR - API url does not exist.")
        return None
    elif r.status_code == 403:
        print("Weather API: ERROR - Forbidden.")
        return None
    elif r.status_code == 401:
        print("Weather API: ERROR - Unauthorized.")
        return None
    elif r.status_code == 429:
        print("Weather API: ERROR - Exceeded maximum API call count.")
        return None
    else:
        print("Weather API: ERROR - Unknown error.")
        return None

def main():
    if len(sys.argv) != 2:
        exit("Usage: {} LOCATION".format(sys.argv[0]))
    location = sys.argv[1]
    record = {}
    api_key = get_api_key()
    weather = get_weather_with_loc(api_key, location)
    if weather is not None:
        weatherList = []
        for weatherInfo in weather:
            weatherList.append(weatherInfo['main'])
        record['weather'] = weatherList;
 
    print(record)
    
if __name__=="__main__":
    main()