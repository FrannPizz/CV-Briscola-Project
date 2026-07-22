import ultralytics

def detect_card(image_path):
    # Load the YOLOv8 model 
    model = ultralytics.YOLO('yolov8n.pt')  # Load the pre-trained YOLOv8 model
