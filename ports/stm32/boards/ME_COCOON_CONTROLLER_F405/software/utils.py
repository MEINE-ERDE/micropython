DEBUG_ENABLED = False


def debug_print(*args, **kwargs):
    if DEBUG_ENABLED:
        print(*args, **kwargs)
