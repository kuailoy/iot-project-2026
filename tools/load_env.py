"""Load local build settings from .env.local into PlatformIO compile defines."""

import ast
from pathlib import Path

Import("env")

if env.IsIntegrationDump():
    Return()


def read_env_file(path):
    values = {}
    if not path.exists():
        return values

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:].lstrip()
        if "=" not in line:
            raise ValueError("Invalid .env.local entry on line {}".format(line_number))

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key not in ("WIFI_SSID", "WIFI_PASSWORD", "TIME_API_KEY", "WEATHER_API_KEY"):
            continue

        if value.startswith(("'", '"')):
            try:
                value = ast.literal_eval(value)
            except (SyntaxError, ValueError) as error:
                raise ValueError("Invalid quoted value in .env.local on line {}".format(line_number)) from error
            if not isinstance(value, str):
                raise ValueError("Expected a string in .env.local on line {}".format(line_number))

        values[key] = value
    return values


project_dir = Path(env.subst("$PROJECT_DIR"))
local_values = read_env_file(project_dir / ".env.local")

env.Append(
    CPPDEFINES=[
        ("WIFI_SSID", env.StringifyMacro(local_values.get("WIFI_SSID", ""))),
        ("WIFI_PASSWORD", env.StringifyMacro(local_values.get("WIFI_PASSWORD", ""))),
        ("TIME_API_KEY", env.StringifyMacro(local_values.get("TIME_API_KEY", ""))),
        ("WEATHER_API_KEY", env.StringifyMacro(local_values.get("WEATHER_API_KEY", ""))),
    ]
)

setting_names = ("WIFI_SSID", "WIFI_PASSWORD", "TIME_API_KEY", "WEATHER_API_KEY")
loaded_names = [name for name in setting_names if local_values.get(name)]
missing_names = [name for name in setting_names if not local_values.get(name)]

if loaded_names:
    print("Loaded from .env.local: " + ", ".join(loaded_names))
else:
    print("No recognized non-empty settings found in .env.local")
if missing_names:
    print("Not set: " + ", ".join(missing_names))
