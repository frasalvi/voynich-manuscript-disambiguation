# ML Project 2: Disambiguating Voynich Manuscript transliterations with word embeddings
### Team members
- Jirka Lhotka
- Francesco Salvi
- Liudvikas Lazauskas

## Usage

The repository contains 3 main notebooks:

- ```embeddings_italian.ipynb```
  Responsible for generating and training model on italian text (Dante's Inferno).
- ```embeddings_latin.ipynb```
  Responsible for generating and training model on latin text (Albert of Aix).
- ```embeddings_voynich.ipynb```
  Responsible for generating and training word embeddings on Voynich Manuscript.

## Dependencies
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

## Resources
- ### Benchmarks
    Used benchmarks can be found in the ```benchmarks/``` folder. Available benchmarks:
  - Latin synonym selection benchmark

- ### Transliterations
    This project uses the transliterations found [here](http://www.voynich.nu/transcr.html#links). There are various transliterations available at this source.

- ### Documentation
    Resources docummenting the usage of IVTFF tools can be found in the ```resources/``` folder.

- ### Transliteration resources 
    Transliteration resources can be found in the ```data/``` folder. They contain 4 different transliterations in the [IVT](http://www.voynich.nu/software/ivtt/IVTFF_format.pdf) file format.

- ### Software
    The software used for filtering and altering the transliterations can be found in ```software/``` folder.

## Text Resources
The texts used in this project can be mainly found in the foler ```texts/```. The folder contains historical texts such as Dante's Inferno and Albert of Aix.

## Helpers

- ### ```baseline.py```
  Used to generate letter frequencies in the text and later predict an expected word when presented with ambiguities.
- ### ```corruptions.py```
  Used to compute probabilities of uncertain and ambiguous characters. It is later used to noise(corrupt) the text when calculating model performance.
- ### ```uncertainties.py```
  Uncertainty class that is able to provide context to a given corrupted word. It can be used to put a word in a context or replace it with a similar word.
- ### ```validation.py```
  Used to evaluate and compute model accuracy. Has methods to predict uncertain words and corruptions.

<em>For more detailed documentation refer to Docstring the file.</em>

## Predictions
The resulting predictions of the model trained on Voynich can be found in the ```predictions/``` folder.