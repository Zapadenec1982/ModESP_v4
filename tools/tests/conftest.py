"""
conftest.py — pytest fixtures для тестування generate_ui.py
"""
import json
import sys
from pathlib import Path

import pytest

# Додаємо tools/ до sys.path для імпорту generate_ui
TOOLS_DIR = Path(__file__).parent.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from generate_ui import (
    ManifestValidator,
    ManifestLoader,
    DriverManifestValidator,
    DriverManifestLoader,
    cross_validate,
    UIJsonGenerator,
    StateMetaGenerator,
    MqttTopicsGenerator,
    DisplayScreensGenerator,
    WIDGET_TYPE_COMPAT,
)

FIXTURES_DIR = Path(__file__).parent / "fixtures"


def load_fixture(name):
    """Load a JSON fixture file by name."""
    path = FIXTURES_DIR / name
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


@pytest.fixture
def validator():
    """Fresh ManifestValidator instance."""
    return ManifestValidator()


@pytest.fixture
def valid_thermostat():
    """Complete valid thermostat manifest."""
    return load_fixture("valid_thermostat.json")


@pytest.fixture
def valid_minimal():
    """Minimal valid manifest (module + state only)."""
    return load_fixture("valid_minimal.json")


@pytest.fixture
def valid_project():
    """Valid project.json."""
    return load_fixture("valid_project.json")


@pytest.fixture
def thermostat_manifests(valid_thermostat):
    """List of manifests containing thermostat (for generators)."""
    return [valid_thermostat]
