# Swarm Learning Object Detection for Autonomous Delivery Vans

This project demonstrates a decentralized swarm learning system for object detection deployed across a fleet of autonomous delivery vans in urban environments. Each van continuously improves its onboard object detection model—identifying pedestrians, cyclists, vehicles, and road obstacles—by sharing encrypted model updates directly with nearby peers, without relying on any central server.

## Research Plan

### 1. Define the Object Detection Task
- **Model Output:** Bounding boxes and class probabilities for objects such as person, bicycle, car, and debris.  
- **Evaluation Metric:** Mean Average Precision (mAP) at IoU thresholds (e.g., 0.5).

### 2. Prepare Hardware & Model
- **Edge Hardware:** Coral Micro board (Edge TPU) integrated into each van’s embedded system.  
- **Model Selection & Compilation:**  
  - Start from an Edge TPU–optimized detector (e.g., `ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite`).  
  - Compile with the Edge TPU Compiler for maximum inference speed.

### 3. Build Per-Van Inference & Update Service
```python
import paho.mqtt.client as mqtt
from pycoral.utils.edgetpu import make_interpreter
from pycoral.adapters import common, detect

# Initialize model and MQTT
interpreter = make_interpreter('ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite')
interpreter.allocate_tensors()
client = mqtt.Client()
client.connect('swarm-mqtt.local', 1883)

while True:
    frame = capture_frame()  # grab camera frame
    common.set_input(interpreter, frame)
    interpreter.invoke()
    detections = detect.get_objects(interpreter, threshold=0.5)

    # Compute weight-delta for swarm update
    delta = compute_weight_delta(interpreter)
    client.publish('swarm/object', delta)
```

### 4. Communication Layer
- **P2P MQTT Mesh:** Each van runs a lightweight MQTT broker over vehicle-to-vehicle Wi-Fi.  
- **Gossip Protocol:** Every 10 minutes, each van randomly selects a small set of nearby peers to exchange encrypted weight-delta updates on the `swarm/object` topic.

### 5. Decentralized Aggregation & Convergence
- **Local Aggregation:** Each van collects incoming deltas, performs a weighted average (e.g., proportional to the number of local samples), applies the update to its model, and resumes inference.  
- **Consensus Formation:** After multiple gossip rounds, the fleet converges on a shared object detection model that adapts to varied city conditions.

### 6. Prototype & Test
1. **Simulated Fleet:** Use Docker containers to emulate multiple vans sharing a virtual MQTT network.  
2. **Field Trials:** Deploy on a real urban route; monitor mAP improvements and end-to-end detection latency.  
3. **Corner Cases:** Verify that rare obstacle detections (e.g., unexpected debris) learned by one van propagate correctly to all others.

### 7. Metrics & Next Steps
- **Detection Accuracy:** mAP before vs. after swarm updates.  
- **Convergence Speed:** Number of gossip rounds needed for stable performance.  
- **Network Overhead:** Bandwidth consumed by delta exchanges.  
- **Resilience:** Impact on model quality when some vans temporarily drop out.

**Future Extensions:**  
- Add a lightweight blockchain layer for tamper-proof update logs and contributor incentives.  
- Organize hierarchical swarms (neighborhood vs. city-wide).  
- Implement adaptive peer selection based on real-time proximity and mobility patterns.