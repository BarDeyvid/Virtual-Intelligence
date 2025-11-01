import os
import time
from llama_cpp import Llama

# --- Configuration ---
# Set the path to the model you downloaded in Step 1
MODEL_PATH = "models/gemma-3-270m-it-F16.gguf"

# Set n_gpu_layers to -1 to offload all model layers to the GPU.
# This is the crucial test to confirm CUDA is working.
# If CUDA is NOT working, the program will still run, but performance will be very slow (on CPU).
N_GPU_LAYERS = -1 
N_CTX = 2048 # Context window size

def main():
    """
    Initializes the Llama model and starts an interactive chat loop.
    Prints performance metrics for each response.
    """
    if not os.path.exists(MODEL_PATH):
        print(f"FATAL Error: Model file not found at: {MODEL_PATH}")
        print("Please ensure you completed Step 1 (model download) correctly.")
        return

    print("--- Initializing Llama Model ---")
    print(f"Model: {MODEL_PATH}")
    print(f"GPU Layers (should be -1 for full offload): {N_GPU_LAYERS}")
    
    try:
        # Load the model, offloading to the GPU
        llm = Llama(
            model_path=MODEL_PATH,
            n_gpu_layers=N_GPU_LAYERS,
            n_ctx=N_CTX,
            verbose=False, # Set to True for Llama.cpp diagnostics
        )
        print("Model loaded successfully!")
    except Exception as e:
        print(f"Error loading model: {e}")
        return

    print("\n--- Interactive Chat Test (Enter 'quit' to exit) ---")
    print("Ask the model a short question (e.g., 'What is the largest planet?'):")

    while True:
        try:
            user_input = input("\n> You: ")
            if user_input.lower() == 'quit':
                break

            if not user_input.strip():
                continue

            # Start timing the generation
            start_time = time.time()
            
            # Generate the response
            stream = llm.create_completion(
                prompt=user_input,
                max_tokens=256,
                stream=True,
                temperature=0.7
            )
            
            full_response = ""
            generated_tokens = 0
            
            print("\n> Model: ", end="", flush=True)
            
            # Stream the response chunks and count tokens
            for output in stream:
                text = output['choices'][0]['text']
                print(text, end="", flush=True)
                full_response += text
                generated_tokens += 1
                
            end_time = time.time()
            elapsed_time = end_time - start_time
            
            # --- Metrics Reporting ---
            if generated_tokens > 0 and elapsed_time > 0:
                tokens_per_second = generated_tokens / elapsed_time
                # The tokens per second metric confirms CUDA performance.
                # If this number is consistently high (e.g., > 10-20 t/s), CUDA is working well.
                print(f"\n\n[Metrics]")
                print(f"Generated Tokens: {generated_tokens}")
                print(f"Time Taken: {elapsed_time:.2f}s")
                print(f"Tokens/Second (t/s): {tokens_per_second:.2f}")
            
        except KeyboardInterrupt:
            print("\nExiting chat loop...")
            break
        except Exception as e:
            print(f"\nAn unexpected error occurred: {e}")
            break

if __name__ == "__main__":
    main()