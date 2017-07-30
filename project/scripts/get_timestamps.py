import os
import re
import argparse

if __name__ == "__main__":

    # Get directory provided by user
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=str, help="Directory to search through")
    args = parser.parse_args()

    # Sort the files
    files = sorted(os.listdir(args.directory))

    # Loop over files getting timestamp
    data = str()
    for _file in files:
        # Create the file name
        file_name = os.path.join(args.directory, _file)

        # Open the file and get data
        with open(file_name, "r") as file_desc:
            data = file_desc.read()

        # Search file for occurence of time stamp string
        match = re.search(r"Timestamp:[\s]+(.*)", data)

        # If there is a match print the data
        if match is not None:
            print(file_name + "," + match.groups()[0] + ",")
