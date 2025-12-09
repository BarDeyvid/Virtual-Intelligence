import torch
import torch.nn as nn
import torch.optim as optim
from sentence_transformers import SentenceTransformer
import torch.onnx
import pandas as pd
import os

# ==========================================
# 1. Configuration & Data Setup
# ==========================================

# Use the full list of experts from your C++ log
EXPERTS = [
    "socialModel", "emotionalModel", "introspectiveModel", 
    "creativeModel", "analyticalModel", "memoryModel", "alyssa"
]
EXPERT_TO_ID = {name: i for i, name in enumerate(EXPERTS)}
NUM_EXPERTS = len(EXPERTS)
EMBEDDING_DIM = 768 # Matches your system's dim

# Choose a 768-dimensional model
EMBEDDING_MODEL_NAME = 'all-mpnet-base-v2' 
DATA_FILE = 'router_training_data.csv'
ONNX_FILENAME = "fusion_router.onnx"

# ==========================================
# 2. Model Definition
# ==========================================

class FusionRouter(nn.Module):
    def __init__(self, input_dim, num_experts):
        super(FusionRouter, self).__init__()
        # The hidden layer size (e.g., 128) can be tuned
        self.network = nn.Sequential(
            nn.Linear(input_dim, 128),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(128, num_experts),
            nn.Softmax(dim=1) 
        )

    def forward(self, x):
        return self.network(x)

# ==========================================
# 3. Training Logic
# ==========================================

def train():
    if not os.path.exists(DATA_FILE):
        print(f"❌ Error: Training data file '{DATA_FILE}' not found.")
        print("Please create the CSV file with 'text' and 'expert_label' columns.")
        return None

    print(f"🔄 Loading data from {DATA_FILE}...")
    df = pd.read_csv(DATA_FILE)
    
    # Map expert names to integer IDs
    df['label_id'] = df['expert_label'].map(EXPERT_TO_ID)
    
    texts = df['text'].tolist()
    labels = df['label_id'].tolist()
    
    print(f"🔄 Loading 768-dim Embedding Model ({EMBEDDING_MODEL_NAME})...")
    embedder = SentenceTransformer(EMBEDDING_MODEL_NAME)
    
    print("🔄 Generating Embeddings...")
    embeddings = embedder.encode(texts)
    
    # Convert to Tensors
    X = torch.tensor(embeddings, dtype=torch.float32)
    y = torch.tensor(labels, dtype=torch.long)
    
    # Initialize Model
    model = FusionRouter(input_dim=EMBEDDING_DIM, num_experts=NUM_EXPERTS)
    
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.005) # Lower LR for stability
    
    print("🚀 Starting Training...")
    model.train()
    for epoch in range(300): # More epochs for better convergence
        optimizer.zero_grad()
        
        outputs = model(X) 
        loss = criterion(outputs, y)
        loss.backward()
        optimizer.step()
        
        if (epoch + 1) % 50 == 0:
            print(f"   Epoch {epoch + 1}: Loss = {loss.item():.4f}")

    print("✅ Training Complete.")
    return model

# ==========================================
# 4. Export to ONNX
# ==========================================

def export_onnx(model):
    model.eval()
    
    # Create a dummy input for the exporter (batch_size=1, vector_dim=768)
    dummy_input = torch.randn(1, EMBEDDING_DIM, requires_grad=True)
    
    print(f"📦 Exporting to {ONNX_FILENAME}...")
    torch.onnx.export(
        model, 
        dummy_input, 
        ONNX_FILENAME, 
        export_params=True, 
        opset_version=11, 
        do_constant_folding=True, 
        input_names=['input_embedding'], 
        output_names=['expert_weights'], 
        dynamic_axes={'input_embedding': {0: 'batch_size'}, 
                      'expert_weights': {0: 'batch_size'}}
    )
    print(f"🎉 Model saved! Check for {ONNX_FILENAME}")

if __name__ == "__main__":
    trained_model = train()
    if trained_model:
        export_onnx(trained_model)