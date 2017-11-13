from setuptools import setup

setup(name='cali_interactive_explorer',
      version='0.1',
      description='Interactive exploration tool for Caliper profiles',
      url='http://github.com/LLNL/Caliper',
      author='David Poliakoff',
      author_email='poliakoff1@llnl.gov',
      license='BSD',
      packages=['cali_interactive_explorer'],
      install_requires=[
                    'pandas',
      ],
      zip_safe=False)
