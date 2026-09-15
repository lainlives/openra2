import subprocess
import sys


def convert_wiki_to_md(input_file: str, output_file: str):
    try:
        # Call pandoc to convert mediawiki format to markdown
        subprocess.run(
            [
                "pandoc",
                "-f",
                "mediawiki",
                "-t",
                "markdown",
                input_file,
                "-o",
                output_file,
            ],
            check=True,
        )
        print(f"Successfully converted '{input_file}' to '{output_file}'")
    except FileNotFoundError:
        print(
            "Error: 'pandoc' is not installed. Please install it via your system package manager."
        )
    except subprocess.CalledProcessError as e:
        print(f"Conversion failed: {e}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python wiki2md.py <input_file.wiki> <output_file.md>")
        sys.exit(1)

    convert_wiki_to_md(sys.argv[1], sys.argv[2])
