# Disambiguating Voynich Manuscript transliterations with word embeddings

Read our [report](Disambiguating%20Voynich%20Manuscript%20transliterations.pdf)!

This work was realised as part of the course "CS-433 Machine Learning" at EPFL, and is the joint effort of of [Francesco Salvi](https://github.com/frasalvi), [Jirka Lhotka](https://github.com/jirkalhotka). and [Liudvikas Lazauskas](https://github.com/Leauyy).


## Repo structure

The repository contains 3 main notebooks aswell as 4 modules:

- ```embeddings_italian.ipynb```
  Responsible for training and evaluating embeddings on italian text (Dante's Inferno).
- ```embeddings_latin.ipynb```
  Responsible for training and evaluating embeddings on latin text (Albert of Aix).
- ```embeddings_voynich.ipynb```
  Responsible for training embeddings on the Voynich Manuscript.
- ```corruptions.py```
  Provide methods to compute ambiguities distributions and to artificially corrupt the texts.
- ```uncertainties.py```
  Provide a class to represent ambiguities with their contexts and methods to create a list of ambiguities given a corrupted text.
- ```baseline.py```
  Provide methods to generate baseline predictions, computing letter frequencies in the text.
- ```validation.py```
  Provide methods to generate predictions and to evaluate the models by computing their accuracy.

## Data
The texts used in this project can be mainly found in the foler ```texts/```. The folder contains historical texts such as Dante's Inferno and Albert of Aix, and Voynich transliterations available [here](http://www.voynich.nu/transcr.html#links). The transliterations are further processed with ivtt, and processed texts are found in the ```data/``` folder.

## Resources
- Benchmarks
    The benchmark used for the Latin synonym selection task can be found in the ```benchmarks/``` folder.

- Software
    The software used for filtering and processing the transliterations can be found in ```software/``` folder, taken from [here](http://www.voynich.nu/software/).
    
- Documentation
    Documentation for the usage of IVTT and IVTFF format can be found in the ```documentation/``` folder.

## Predictions
The resulting predictions of the model trained on Voynich can be found in the ```predictions/``` folder.

## Requirements
- ### [Gensim Models](https://radimrehurek.com/gensim/auto_examples/index.html##documentation)
    - version: ```4.1.2```
    - package name ``` gensim```
- ### [NumPy](https://numpy.org/devdocs/index.html)
  - version: ```1.19.5``` 
  - package name ``` numpy```
- ### [SciPy](https://scipy.org/)
  - version: ```1.7.3```
  - package name ``` scipy```
- ### [Natural Language Toolkit](https://www.nltk.org/#natural-language-toolkit)
  - version: ```3.6.5```
  - package name ```nltk ```
- ### [Smart Open](https://pypi.org/project/smart-open/)
  - version: ```5.2.1```
  - package name ```smart-open```
- ### [The Classical Language Toolkit](http://cltk.org/)
  - version: ```1.0.21```
  - package name ```cltk```
