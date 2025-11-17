"""
Setup configuration for SRE HIL testing platform.

Install in development mode:
    pip install -e .

Install from source:
    pip install .
"""

from setuptools import setup, find_packages
from pathlib import Path

# Read version from version.py
version_file = Path(__file__).parent / 'sre_hil' / 'version.py'
version = {}
with open(version_file) as f:
    exec(f.read(), version)

# Read long description from README
readme_file = Path(__file__).parent / 'README.md'
long_description = ""
if readme_file.exists():
    with open(readme_file, encoding='utf-8') as f:
        long_description = f.read()

setup(
    name="sre-hil",
    version=version['__version__'],
    description="Hardware-in-the-Loop Testing Platform for Spartan Racing Electric VCU",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Spartan Racing Electric - Controls Team",
    author_email="",
    url="https://github.com/spartanracingelectric/VCU",
    packages=find_packages(),
    install_requires=[
        "python-can>=4.0.0",
        "cantools>=36.0.0",
        "click>=8.0.0",
        "rich>=13.0.0",
        "pytest>=7.0.0",
        "pyyaml>=6.0",
    ],
    extras_require={
        'dev': [
            'pytest-html>=3.0.0',
            'pytest-cov>=3.0.0',
        ],
    },
    entry_points={
        'console_scripts': [
            'hil=sre_hil.cli.main:cli',
        ],
    },
    python_requires='>=3.8',
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Testing",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
    ],
)
