import os

# Lista dos arquivos CSV que precisam ser corrigidos
arquivos_para_corrigir = {
    "my_dataset/metadata_train_ja.csv": "ja",
    "my_dataset/metadata_val_ja.csv": "ja",
    "my_dataset/metadata_train_en.csv": "en",
    "my_dataset/metadata_val_en.csv": "en",
}

print("Iniciando a correção dos arquivos CSV...")

for nome_arquivo, lang_code in arquivos_para_corrigir.items():
    if not os.path.exists(nome_arquivo):
        print(f"AVISO: Arquivo '{nome_arquivo}' não encontrado. Pulando.")
        continue

    linhas_corrigidas = []
    with open(nome_arquivo, 'r', encoding='utf-8') as f:
        for linha in f:
            linha = linha.strip()
            if '|' in linha:
                # Adiciona o código do idioma como a terceira coluna
                linhas_corrigidas.append(f"{linha}|{lang_code}\n")

    # Sobrescreve o arquivo com o conteúdo corrigido
    with open(nome_arquivo, 'w', encoding='utf-8') as f:
        f.writelines(linhas_corrigidas)

    print(f"Arquivo '{nome_arquivo}' foi corrigido com sucesso!")

print("\nCorreção concluída.")