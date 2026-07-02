from fastapi import FastAPI
import pandas as pd
import os

app = FastAPI()

CSV_FILE = "sensor_data.csv"


@app.get("/")
def home():

    return {
        "message": "Smart Agriculture API Running"
    }


@app.get("/latest")
def latest():

    if not os.path.exists(CSV_FILE):

        return {
            "message": "No data"
        }

    df = pd.read_csv(CSV_FILE)

    if len(df) == 0:

        return {
            "message": "No data"
        }

    return df.iloc[-1].to_dict()


@app.get("/history")
def history(limit: int = 100):

    if not os.path.exists(CSV_FILE):

        return []

    df = pd.read_csv(CSV_FILE)

    return df.tail(limit).to_dict(
        orient="records"
    )


@app.get("/stats")
def stats():

    if not os.path.exists(CSV_FILE):

        return {
            "total_records": 0
        }

    df = pd.read_csv(CSV_FILE)

    return {
        "total_records": len(df)
    }
