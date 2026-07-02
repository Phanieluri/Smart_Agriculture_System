# train_random_forest.py

import pandas as pd
import joblib

from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

df = pd.read_csv("dataset.csv")

X = df[["moisture", "temperature", "humidity"]]
y = df["label"]

X_train, X_test, y_train, y_test = train_test_split(
    X,
    y,
    test_size=0.2,
    random_state=42
)

model = RandomForestClassifier()

model.fit(X_train, y_train)

predictions = model.predict(X_test)

accuracy = accuracy_score(y_test, predictions)

print(f"Random Forest Accuracy: {accuracy}")

joblib.dump(model, "random_forest_model.pkl")

with open("rf_accuracy.txt", "w") as f:
    f.write(str(accuracy))