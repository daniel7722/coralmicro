import tensorflow as tf
import numpy as np
import cv2
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.applications.mobilenet_v2 import preprocess_input
from tensorflow.keras.models import Model



# Load MobileNetV2 pre-trained on ImageNet
base_model = MobileNetV2(weights="imagenet", include_top=False, input_shape=(224, 224, 3), pooling='avg')

# Define the face embedding model
embedding_model = Model(inputs=base_model.input, outputs=base_model.output)

# Print model summary
embedding_model.summary()
