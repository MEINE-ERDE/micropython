from pyb import repl_uart, main
from os import listdir
from sys import print_exception


def boot():
    with_repl = "with_repl"
    # try:
    #     if with_repl in listdir("/flash"):
    #         print("# Keeping UART REPL")
    #     else:
    #         repl_uart(None)  # Disable UART REPL

    # except Exception as e:
    #     print_exception(e)

    main("main.py")


if __name__ == "__main__":
    boot()
