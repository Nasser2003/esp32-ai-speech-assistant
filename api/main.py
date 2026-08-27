from flask import Flask, request, jsonify, Response
import ollama
from ollama_service import generate

app = Flask(__name__)

# variables
client = ollama.Client()
model = "llama3.2"

url = "127.0.0.1:11434/api/chat"



@app.route('/ask', methods=['POST'])
def ask():
    data = request.get_json()
    question = data.get('question')
    
    # send prompt to the model
    return Response(
        generate(client, model, question),
        mimetype="text/plain"
    )

@app.route('/', methods=['GET'])
def home():
    return "Welcome to the AI Question Answering API!"

if __name__ == '__main__':
    app.run(debug=True, host="0.0.0.0", port=5000)