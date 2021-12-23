import re
from collections import Counter
import bisect
import random


def get_letter_probabilities(text, uncertainty_chars):
    '''
    Compute the frequency distribution of letters in the text, accounting
    half an occurrence for options in alternate readings.

    Args:
        text (str): input text 
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)

    Returns:
        (dict of (str: float)): mapping character -> frequency
    '''

    # Replace all uncertain spaces with actual spaces
    text = text.replace(uncertainty_chars['UNCERTAIN_SPACE'], ' ')

    # Find all occurrences of alternative options
    alternate_options = re.findall("\[(.*?):(.*?)\]", text)
    alternate_options = [letter for tupl in alternate_options for letter in tupl]

    # Remove alternate readings and single uncertainties
    text = re.sub("\[(.*?):(.*?)\]", '', text)
    text = re.sub(uncertainty_chars['SINGLE_UNCERTAINTY'].replace('?', '\?'), '', text)

    all_certain_letters_list = [letter for letter in text.replace(' ', '')]
    all_uncertain_letters_list = alternate_options
    letter_number = len(all_certain_letters_list) + len(all_uncertain_letters_list)/2
    uncertain_counter = Counter(all_uncertain_letters_list)
    certain_counter = Counter(all_certain_letters_list)

    # Divide by 2 the counts of the uncertain letters
    for key in uncertain_counter:
        uncertain_counter[key] /= 2

    combined_counter = uncertain_counter + certain_counter
    letter_set = list(combined_counter.keys())
    letter_occurences = list(combined_counter.values())
    letter_probabilities = [letter / letter_number for letter in letter_occurences]

    letter_prob_dict = dict(zip(letter_set, letter_probabilities))
    return letter_prob_dict


def get_random_letter(alternatives, probabilities):
    '''
    Extract a random letter from a list of alternatives, with
    respective probabilities.

    Args:
        alternatives (list of str): possibilities for the extraction
        probabilities (list of float): probabilities of each possibility
    '''

    # Get CDF given letter probabilities
    cdf = []
    total = sum(probabilities)
    cumprob = 0
    for prob in probabilities:
        cumprob += prob
        cdf.append(cumprob / total)

    extraction = random.random()
    idx = bisect.bisect(cdf, extraction)

    return alternatives[idx]


def predict_uncertain_word_baseline(uncertain_word, uncertainty_chars, letter_dictionary):
    '''
    Replace an uncertain word randomly, according to the frequency
    distribution of characters in the language.

    Args:
        uncertain_word (str): a word containing uncertainties
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        letter_dictionary (dict of (str: float)): mapping character -> frequency
 
    Returns:
        (str): output prediction
    '''

    predicted_word = uncertain_word

    # Resolve all alternate readings
    alt_readings = re.findall('\[(.*?):(.*?)\]', uncertain_word)
    for alt_reading in alt_readings:
        alternatives = [alt_reading[0], alt_reading[1]]
        probas = [letter_dictionary[alt] for alt in alternatives]
        choice = get_random_letter(alternatives, probas)
        predicted_word = re.sub('\[(.*?):(.*?)\]', choice, predicted_word, 1)

    # Resolve all spaces
    uncertain_spaces = re.findall(uncertainty_chars['UNCERTAIN_SPACE'], predicted_word)
    for _ in uncertain_spaces:
        alternatives = [' ', '']
        probas = [0.5, 0.5]
        choice = get_random_letter(alternatives, probas)
        predicted_word = re.sub(uncertainty_chars['UNCERTAIN_SPACE'], choice, predicted_word, 1)

    # Resolve single uncertainties
    single_uncertainties = re.findall(uncertainty_chars['SINGLE_UNCERTAINTY'].replace('?', '\?'), predicted_word)
    for _ in single_uncertainties:
        alternatives = list(letter_dictionary.keys())
        probas = list(letter_dictionary.values())
        choice = get_random_letter(alternatives, probas)
        predicted_word = re.sub(uncertainty_chars['SINGLE_UNCERTAINTY'].replace('?', '\?'), choice, predicted_word, 1)

    return predicted_word


def predict_corruptions_baseline(uncertainties_list, uncertainty_chars, letter_dictionary):
    '''
    Apply the baseline, predicting artificially corrupted words according
    to the frequency distribution of characters in the language.

    Args:
        uncertainties_list (list of Uncertainty): list of uncertainties in the text
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        letter_dictionary (dict of (str: float)): mapping character -> frequency

    Returns:
        (list of (str, float): accuracy over the different types of uncertainties.        
    '''

    results = list(map(lambda uncertainty: (
        uncertainty.correct_word,
        predict_uncertain_word_baseline(uncertainty.corrupted_word, uncertainty_chars, letter_dictionary),
        uncertainty.uncertainty_types),
                       uncertainties_list))

    y, predictions, types = list(zip(*results))

    return y, predictions, types
