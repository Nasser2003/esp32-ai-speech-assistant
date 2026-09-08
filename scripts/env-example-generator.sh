#!/bin/bash

set -e

# Usage:
# ./update-env-example.sh <env_file>

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <env_file>"
    exit 1
fi

ENV_FILE="$1"
EXAMPLE_FILE="${ENV_FILE}.example"

if [ ! -f "$ENV_FILE" ]; then
    echo "Error: $ENV_FILE does not exist"
    exit 1
fi

if [ ! -f "$EXAMPLE_FILE" ]; then
    touch "$EXAMPLE_FILE"
fi

TMP_FILE=$(mktemp)

while IFS= read -r line || [[ -n "$line" ]]; do

    # Copier les lignes vides et commentaires
    if [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]]; then
        echo "$line" >> "$TMP_FILE"
        continue
    fi

    # Ignorer les lignes qui ne sont pas des variables
    if [[ "$line" != *=* ]]; then
        continue
    fi

    # Récupérer le nom de la variable
    key="${line%%=*}"

    # Si la variable existe déjà dans .env.example,
    # conserver sa valeur
    if grep -q "^${key}=" "$EXAMPLE_FILE"; then
        grep "^${key}=" "$EXAMPLE_FILE" >> "$TMP_FILE"
    else
        # Nouvelle variable : valeur vide
        echo "${key}=" >> "$TMP_FILE"
    fi

done < "$ENV_FILE"

mv "$TMP_FILE" "$EXAMPLE_FILE"

echo "Updated: $EXAMPLE_FILE"