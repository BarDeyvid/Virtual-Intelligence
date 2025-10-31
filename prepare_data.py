import os
import pandas as pd
from sklearn.model_selection import train_test_split

def prepare_metadata(
    metadata_file: str,
    output_dir: str,
    language: str = "ja",  # Idioma padrão para o dataset
    split_ratio: float = 0.1,
    random_state: int = 42,
):
    """
    Carrega o arquivo de metadados, adiciona a coluna de idioma e o divide
    em arquivos de treino e validação.

    Args:
        metadata_file (str): O caminho para o arquivo de metadados principal (ex: metadata_fixed.csv).
        output_dir (str): O diretório onde os novos arquivos serão salvos.
        language (str): O idioma padrão a ser atribuído a todas as amostras.
        split_ratio (float): A proporção do dataset a ser usada para validação.
                             (ex: 0.1 = 10% para validação, 90% para treino).
        random_state (int): Semente para a divisão, garante que o resultado seja o mesmo a cada execução.
    """
    # Garante que o diretório de saída exista
    os.makedirs(output_dir, exist_ok=True)

    # Carrega o arquivo de metadados
    df = pd.read_csv(metadata_file, sep="|", header=None)
    
    # Adiciona a coluna de idioma
    # A estrutura padrão do LJSpeech formatter é `audio_file|transcription`
    # Adicionamos uma terceira coluna para o idioma
    df[2] = language
    
    # Renomeia as colunas para clareza
    df.columns = ["audio_file", "text", "language"]

    print(f"Número total de amostras: {len(df)}")

    # Divide o dataset em treino e validação
    df_train, df_val = train_test_split(df, test_size=split_ratio, random_state=random_state)

    print(f"Amostras para treino: {len(df_train)}")
    print(f"Amostras para validação: {len(df_val)}")

    # Salva os novos arquivos de metadados
    df_train.to_csv(os.path.join(output_dir, "metadata_train.csv"), sep="|", index=False, header=False)
    df_val.to_csv(os.path.join(output_dir, "metadata_val.csv"), sep="|", index=False, header=False)

    print("Arquivos metadata_train.csv e metadata_val.csv criados com sucesso!")

if __name__ == "__main__":
    # Caminho para o seu arquivo de metadados original
    input_metadata_file = "my_dataset/metadata_fixed.csv"
    
    # Diretório onde os arquivos de saída serão salvos
    output_dir = "my_dataset"
    
    # Execute a função para preparar os dados
    # Mude 'ja' para 'en' ou o idioma que seu dataset possui
    prepare_metadata(input_metadata_file, output_dir, language="ja")