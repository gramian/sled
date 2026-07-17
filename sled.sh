#!/bin/sh

DOSBOX=""

for candidate in dosbox dosbox-x dosbox-staging; do
    if command -v "$candidate" >/dev/null 2>&1; then
        DOSBOX="$candidate"
        break
    fi
done

if [ -z "$DOSBOX" ]; then
    echo "Error: No DOSBox found (dosbox, dosbox-x, or dosbox-staging)" >&2
    exit 1
fi

$DOSBOX -c "mount q $(pwd)" -c "q:" -c "sled"
