import numpy as np
import tflite_runtime.interpreter as tflite
import paho.mqtt.client as mqtt
import json

mqtt_broker = 'emqx'  
mqtt_port = 1883
mqtt_topic = 'model/data'

model_path = 'model.tflite'
interpreter = tflite.Interpreter(model_path=model_path)
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

class_labels = ['cat', 'dog', 'person']

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(mqtt_topic)
    else:
        print(f"Failed to connect to MQTT broker, return code {rc}")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload)
        print(f"Received data: {data}")

        
        img_path = data['image_path']
        img = cv2.imread(img_path)
        img = cv2.resize(img, (224, 224))
        img = img / 255.0
        img_array = np.expand_dims(img, axis=0).astype(np.float32)

        interpreter.set_tensor(input_details[0]['index'], img_array)

        interpreter.invoke()

        output_data = interpreter.get_tensor(output_details[0]['index'])
        predicted_label_index = np.argmax(output_data)
        predicted_label = class_labels[predicted_label_index]

        print(f"Predicted label: {predicted_label}")

        if predicted_label == 'person':
            detection_data = {
                "detected": "true"
            }
            client.publish('detection_topic', json.dumps(detection_data))
            print(f"Published to detection_topic: {detection_data}")

    except Exception as e:
        print(f"Error processing message: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(mqtt_broker, mqtt_port, 60)

client.loop_forever()
