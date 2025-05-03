
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Read results
results = pd.read_csv('results.csv')

# Set up the figure size
plt.figure(figsize=(12, 8))

# Find metrics
metrics = results['Metric'].tolist()
test_configs = results.columns[1:]

# Create a bar chart for each metric
for i, metric in enumerate(metrics):
    metric_data = results.iloc[i, 1:].values
    
    # Skip if all values are NaN
    if np.isnan(metric_data).all():
        continue
    
    plt.figure(figsize=(10, 6))
    bars = plt.bar(test_configs, metric_data)
    plt.title(f'{metric} Comparison')
    plt.ylabel(metric)
    plt.xticks(rotation=45, ha='right')
    
    # Add values on top of bars
    for bar in bars:
        height = bar.get_height()
        if not np.isnan(height):
            plt.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.2f}',
                    ha='center', va='bottom')
    
    plt.tight_layout()
    plt.savefig(f'{metric.replace(" ", "_")}.png')
    plt.close()

print("Plots saved to current directory")
