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
        self.model = fasttext.load_model(model_path.as_posix())
        

    def detect(self, text: str, lang_threshold: float = 0.7, probable_lang: str = "en") -> str:
        clean_text = " ".join(text.split())
        predictions = self.model.predict(clean_text, k=5)  # Get the first top prediction (k=1)
        
        # print(f"[LANGUAGE DETECTOR] Detected language: {predictions} for text: {clean_text}")
        if predictions and len(predictions[0]) > 0:
            if predictions[1][0] >= lang_threshold:
                return predictions[0][0].replace("__label__", "")
            else:
                print(f"[LANGUAGE DETECTOR] Low confidence ({predictions[1][0]:.2f}) for text: {clean_text}. Using probable language: {probable_lang}")
                return probable_lang
        
        raise ValueError("Could not detect language for the given text.")