import csv

input_path = "my_dataset\\metadata.csv"
output_path = "my_dataset\\metadata_fixed.csv"
speaker = "speaker1"

with open(input_path, encoding="utf-8") as infile, open(output_path, "w", encoding="utf-8", newline="") as outfile:
    reader = csv.reader(infile, delimiter="|")
    writer = csv.writer(outfile, delimiter="|")
    header = next(reader)
    if len(header) == 2:
        header.append("speaker")
    writer.writerow(header)
    for row in reader:
        if len(row) == 2:
            row.append(speaker)
        writer.writerow(row)

print(f"Fixed metadata written to {output_path}")
