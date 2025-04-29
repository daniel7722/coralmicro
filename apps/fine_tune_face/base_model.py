import tensorflow as tf
import numpy as np
import os
import pandas as pd
import cv2
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.applications.mobilenet_v2 import preprocess_input
from tensorflow.keras.models import Model
from tensorflow.keras.preprocessing.image import load_img, img_to_array

from sklearn.svm import SVC
from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split


# Paths
DATA_DIR = './data/img_align_celeba'
ATTR_FILE = './data/attributes/list_attr_celeba.txt'

# Image size and batch size
IMG_SIZE = (224, 224)
BATCH_SIZE = 32

# Load attributes
attributes = pd.read_csv(ATTR_FILE, sep="\s+", skiprows=1)
attributes.replace(to_replace=-1, value=0, inplace=True)

# Create Tensorflow dataset
def load_image(filename, label): 
    img = tf.io.read_file(filename)
    img = tf.image.decode_jpeg(img, channels=3)
    img = tf.image.resize(img, IMG_SIZE)
    img = img /255.0
    return img, label

def get_dataset(data_dir, attributes, batch_size): 
    filepaths = [os.path.join(data_dir, img) for img in attributes.index]
    labels = attributes[]

# Load MobileNetV2 pre-trained on ImageNet
mobile_net = MobileNetV2(weights="imagenet", include_top=False, input_shape=(224, 224, 3), pooling='avg')

# Print model summary
mobile_net.summary()


def preprocess_image(image_path):
    """Preprocess the input image for the model."""
    img = load_img(image_path, target_size=(224, 224))
    img_array = img_to_array(img)

    # Noramlise pixel values to [0, 1]
    img_array = img_array / 255.0
    img_array = np.expand_dims(img_array, axis=0) # Add batch dimension
    return img_array

def generate_embedding(image_path, model): 
    """Generate an embedding for a given image."""
    preprocessed_image = preprocess_image(image_path)
    embedding = model.predict(preprocessed_image)
    return embedding





    


