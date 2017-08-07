import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("start", type=float)
    parser.add_argument("stop", type=float)
    parser.add_argument("--classes", default=20, type=int)

    args = parser.parse_args()

    step = (args.stop - args.start) / args.classes

    while args.start < args.stop:
        print(args.start)
        args.start += step
