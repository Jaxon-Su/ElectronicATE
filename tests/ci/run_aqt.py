"""aqt CLI using the OS certificate trust store, with TLS/hash checks enabled."""
import sys
import truststore

# This is a CLI entrypoint. Early injection also runs in Windows spawn children.
sys.dont_write_bytecode = True
truststore.inject_into_ssl()

if __name__ == '__main__':
    from aqt.__main__ import main
    sys.exit(main())
