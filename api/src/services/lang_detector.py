import fasttext
from pathlib import Path
from urllib.request import urlretrieve

class LanguageDetector:
    def __init__(self, path: str):
        model_path = Path(path)
        model_path.parent.mkdir(parents=True, exist_ok=True)

        url = "https://dl.fbaipublicfiles.com/fasttext/supervised-models/lid.176.bin"

        if not model_path.exists():
            print("[LANGUAGE DETECTOR] Downloading model...")
            urlretrieve(url, model_path)
            print(f"[LANGUAGE DETECTOR] Model downloaded: {model_path}")
            
        self.model = fasttext.load_model(model_path)
        

    def detect(self, text: str) -> str:
        predictions = self.model.predict(text, k=1)  # Get the first top prediction (k=1)
        
        if predictions and len(predictions[0]) > 0:
            return predictions[0][0].replace("__label__", "")
        
        raise ValueError("Could not detect language for the given text.")