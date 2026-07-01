from setuptools import setup, find_packages

setup(
    name="traflight",
    version="1.0.0",
    description="🚦 LLM Traffic Light — ESP8266 TFT traffic light CLI for AI Agent",
    py_modules=["traflight"],
    python_requires=">=3.7",
    install_requires=["pyserial>=3.5"],
    entry_points={
        "console_scripts": [
            "traflight=traflight:main",
        ],
    },
)
