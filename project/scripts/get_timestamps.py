import os
import re
import argparse

from decimal import Decimal

if __name__ == "__main__":

    parser = argparse.ArgumentParser()

    parser.add_argument("directory", type=str, help="Directory to search through")

    args = parser.parse_args()

    files = sorted(os.listdir(args.directory))


    for _file in files:
        data = ""
        file_name = os.path.join(args.directory, _file)
        with open(file_name, "r") as file_desc:
            data = file_desc.read()

        match = re.search(r"Timestamp:[\s]+(.*)", data)

        if match is not None:
            print(file_name + "," + match.groups()[0] + ",")
