# compare_models.py

import shutil

# Read accuracies
with open("rf_accuracy.txt", "r") as f:
    rf_acc = float(f.read())

with open("dt_accuracy.txt", "r") as f:
    dt_acc = float(f.read())

with open("svm_accuracy.txt", "r") as f:
    svm_acc = float(f.read())

accuracies = {
    "random_forest_model.pkl": rf_acc,
    "decision_tree_model.pkl": dt_acc,
    "svm_model.pkl": svm_acc
}

print("\nModel Accuracies:")
for model, acc in accuracies.items():
    print(f"{model} -> {acc}")

# Select best model
best_model = max(accuracies, key=accuracies.get)

print(f"\nBest Model: {best_model}")

# Save best model
shutil.copy(best_model, "best_model.pkl")

print("Best model saved as best_model.pkl")