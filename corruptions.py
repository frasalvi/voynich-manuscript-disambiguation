import re
import random


def calculate_uncertainties_statistics(text, uncertainty_chars):
    '''
    Computes the probabilities of different types of uncertainties in the
    given text.

    Args:
        text (str): original text
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)

    Returns:
        dict: mapping of (type of uncertainty, probability)
    '''

    parenthesis = re.findall('\[.*?\]', text)

    # replace all square brackets (alternate readings) with a one-char representation
    text_wpar = re.sub('\[(^\])+?\]', uncertainty_chars['ALTERNATE_CHAR'], text)

    single_unc = uncertainty_chars['SINGLE_UNCERTAINTY'].replace('?', '\?')
    unc_seq = uncertainty_chars['UNCERTAIN_SEQUENCE'].replace('?', '\?')

    qmarks = re.findall('[^'+single_unc+']('+single_unc+')[^'+single_unc+']', text_wpar)
    qmarks2 = re.findall('[^'+single_unc+']('+single_unc+single_unc+')[^'+single_unc+']', text_wpar)
    qmarks3 = re.findall(unc_seq, text_wpar)
    maybe_spaces = re.findall(uncertainty_chars['UNCERTAIN_SPACE'], text_wpar)

    # Count number of word characters and space-like characters
    nchars = len(re.findall(f"[^ {uncertainty_chars['UNCERTAIN_SPACE']}\n]", text_wpar))
    nspaces = len(re.findall(f"[ {uncertainty_chars['UNCERTAIN_SPACE']}]", text_wpar))

    alternate_readings_ratio = len(parenthesis) / nchars
    single_uncertainty_ratio = len(qmarks + qmarks2) / nchars
    uncertain_sequences_ratio = len(qmarks3) / nchars
    space_uncertainty_ratio = len(maybe_spaces) / nspaces

    print(f'Number of alternate readings: {len(parenthesis)}, {alternate_readings_ratio*100:.3f}% of chars')
    print(f'Number of single uncertainty: {len(qmarks+qmarks2)}, {single_uncertainty_ratio*100:.3f}% of chars')
    print(f'Number of uncertain sequences: {len(qmarks3)}, {uncertain_sequences_ratio*100:.3f}% of chars')
    print(f'Number of uncertain spaces: {len(maybe_spaces)}, {space_uncertainty_ratio*100:.3f}% of spaces')

    uncertainty_ratios = {'ALTERNATE_READINGS_RATIO': alternate_readings_ratio,
                          'SINGLE_UNCERTAINTY_RATIO': single_uncertainty_ratio,
                          'UNCERTAIN_SEQUENCE_RATIO': uncertain_sequences_ratio,
                          'SPACE_UNCERTAINTY_RATIO': space_uncertainty_ratio
                          }

    return uncertainty_ratios


def get_space_transform_probability(uncertainty_ratios, uncertainty_chars, actual_space_ratio, space_count, middle_char_count):
    '''
    Compute the probabilities of corrupting spaces and non-space chars to
    uncertain spaces.

    Args:
        uncertainty_ratios (dict): mapping of (type of uncertainty, probability)
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        actual_space_ratio (float): ratio of actual spaces out of all the uncertain spaces.
        space_count (int): number of spaces in the original text.
        middle_char_count (int): number of characters in the original text, excluding spaces and chars at the beginning of a word.

    Returns:
        float: probability of a space being corrupted to an uncertain space.
        float: probability of a non-space char being corrupted to an uncertain space.
    '''

    uncertain_space_count = uncertainty_ratios['SPACE_UNCERTAINTY_RATIO'] * space_count / \
        (1 - uncertainty_ratios['SPACE_UNCERTAINTY_RATIO'])
    uncertain_actual_space_count = uncertain_space_count * actual_space_ratio
    uncertain_fake_space_count = uncertain_space_count * (1 - actual_space_ratio)

    space_to_uncertain_space_proba = uncertain_actual_space_count / space_count
    char_to_uncertain_space_proba = uncertain_fake_space_count / middle_char_count

    return space_to_uncertain_space_proba, char_to_uncertain_space_proba


def transform_space(uncertainty_chars, space_to_uncertain_space_proba):
    '''
    Trasform a space to an uncertain space according to the specified probability.

    Args:
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        space_to_uncertain_space_proba (float): probability of a space being corrupted to an uncertain space.

    Returns:
        str: resulting character (a space or an uncertain space)
    '''

    if(random.random() <= space_to_uncertain_space_proba):
        return uncertainty_chars['UNCERTAIN_SPACE']
    else:
        return ' '


def transform_char(char, uncertainty_ratios, uncertainty_chars, alphabet, char_to_uncertain_space_proba):
    '''
    Trasform a non-space character according to uncertainty_ratios.

    Args:
        char (str): original character
        uncertainty_ratios (dict): mapping of (type of uncertainty, probability)
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        alphabet (list): known non-space characters in the alphabet
        char_to_uncertain_space_proba (float): probability of a non-space char being corrupted to an uncertain space.

    Returns:
        str: resulting character
    '''

    extraction = random.random()

    # Probability already tested in previous if statements
    tested_prob = 0

    if extraction <= tested_prob + char_to_uncertain_space_proba:
        return uncertainty_chars['UNCERTAIN_SPACE'] + char

    tested_prob += char_to_uncertain_space_proba

    if extraction <= tested_prob + uncertainty_ratios['ALTERNATE_READINGS_RATIO']:
        # Generate a random letter and have it as an alternative toghether
        # with the real letter.

        alt_char = random.choice(alphabet)
        seq = [char, alt_char]
        random.shuffle(seq)
        return f"[{':'.join(seq)}]"

    tested_prob += uncertainty_ratios['ALTERNATE_READINGS_RATIO']

    if extraction <= tested_prob + uncertainty_ratios['SINGLE_UNCERTAINTY_RATIO']:
        return uncertainty_chars['SINGLE_UNCERTAINTY']

    tested_prob += uncertainty_ratios['SINGLE_UNCERTAINTY_RATIO']

    if extraction <= tested_prob + uncertainty_ratios['UNCERTAIN_SEQUENCE_RATIO']:
        return uncertainty_chars['UNCERTAIN_SEQUENCE']

    return char


def corrupt_text(text, uncertainty_ratios, uncertainty_chars, alphabet, actual_space_ratio):
    '''
    Corrupt the text according to the specified uncertainties probabilities and
    representations, assuming uncertain spaces are actual spaces with probability
    equal to actual_space_ratio.

    Args:
        text (str): original text
        uncertainty_ratios (dict): mapping of (type of uncertainty, probability)
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        alphabet (list): known non-space characters in the alphabet
        actual_space_ratio (float): ratio of actual spaces out of all the uncertain spaces.

    Returns:
        (str): corrupted text
    '''

    # space_count, char_count = count_space_chars(text, alphabet)
    space_count = len(re.findall(' ', text))
    middle_char_count = len(re.findall('(?<=[^ ])['+alphabet+']', text))
    probas = get_space_transform_probability(uncertainty_ratios, uncertainty_chars,
                                             actual_space_ratio, space_count, middle_char_count)
    space_to_uncertain_space_proba = probas[0]
    char_to_uncertain_space_proba = probas[1]

    corrupted_text = '-'

    for char in text:
        if char == ' ':
            corrupted_text += transform_space(uncertainty_chars, space_to_uncertain_space_proba)

        elif char in alphabet:

            # For characters at the beginning of a word, remove the probability
            # of corruption to uncertain spaces.
            if(corrupted_text[-1] in f' {uncertainty_chars["UNCERTAIN_SPACE"]}\n'):
                corrupted_text += transform_char(char, uncertainty_ratios,  uncertainty_chars,
                                                alphabet, 0)
            else:
                corrupted_text += transform_char(char, uncertainty_ratios,  uncertainty_chars,
                                                 alphabet, char_to_uncertain_space_proba)
        else:
            corrupted_text += char

    corrupted_text = corrupted_text[1:]
    calculate_uncertainties_statistics(corrupted_text, uncertainty_chars)

    return corrupted_text
