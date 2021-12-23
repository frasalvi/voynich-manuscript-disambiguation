import re
import itertools


ALT_READING = r'\[(\S*?):(\S*?)\]'


class Uncertainty:
    '''
    Istance of a word with containing at least one uncertainty

    Attributes:
        corrupted_word (str): the corrupted word
        correct_word (str): the correct word before corruption
        left_context (list of str): context to the left of the word in the sentence
        right_context (list of str): context to the right of the word in the sentence
        alternatives_list (list of str): list of alternatives for replacing the uncertainty
        uncertainty_type  (set of str): types of uncertainties appearing in the word
    
    Note:
        uncertainty_type :: 'uncertain_space' | 'alt_reading' | 'single_uncertainty' | 'multiple_uncertainty'
    '''

    def __init__(self, correct_word, left_context, right_context, uncertainty_types, alternatives_list, corrupted_word):
        self.corrupted_word = corrupted_word
        self.correct_word = correct_word
        self.left_context = left_context
        self.right_context = right_context
        self.alternatives_list = alternatives_list
        self.uncertainty_types = uncertainty_types

    def __str__(self):
        return '{' + 'corrupted_word: ' + str(self.corrupted_word)+ ',\n' + \
                     'correct_word: ' + str(self.correct_word)+ ',\n' + \
                     'left_context: ' + str(self.left_context)+ ',\n' + \
                     'right_context: ' + str(self.right_context)+ ',\n' + \
                     'uncertainty_types: ' + str(self.uncertainty_types)+ ',\n' + \
                     'alternatives_list: ' + str(self.alternatives_list)+ '}\n\n\n'

    def __repr__(self):
        return self.__str__()

    def asdict(self):
        return  {'corrupted_word': self.corrupted_word,
                 'correct_word': self.correct_word,
                 'left_context': self.left_context,
                 'right_context': self.right_context,
                 'alternatives_list': self.alternatives_list,
                 'uncertainty_types': self.uncertainty_types
                 }


def has_uncertainty(word, uncertainty_chars):
    '''
    Analyze whether the word has an uncertainty.

    Args:
        word (str): input word
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)

    Returns:
        (bool): True if there is an uncertainty, False otherwise
    '''

    return len(re.findall(ALT_READING, word)) > 0 or \
        any(map(lambda unc: unc in word, uncertainty_chars.values()))


def create_alternatives(uncertain_word, uncertainty_chars, alphabet):
    '''
    Create a list of possible alternatives for a uncertain word.

    Args:
        uncertain_word (str): input uncertain word
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        alphabet (list): known non-space characters in the alphabet

    Returns:
        (list): alternatives for replacing the uncertainty.
    '''

    letters = [letter for letter in alphabet]
    # Create all possible replacements of multiple uncertainties,
    # considering replacements from 1 to 3 characters.
    double_uncertainty_replacements = (''.join(y) for y in
                                       itertools.product(letters, repeat=2))
    double_uncertainty_replacements = list(double_uncertainty_replacements)
    multiple_uncertainty_replacements = (''.join(y) for y in
                                         itertools.product(letters, repeat=3))
    multiple_uncertainty_replacements = list(multiple_uncertainty_replacements)
    multiple_uncertainty_replacements += double_uncertainty_replacements + letters

    # Iteratively, go through a nested list of tentative alternatives and resolve
    # ambiguities adding all possible replacements. For each uncertain space,
    # processed before any other ambiguity, an internal list is added to the external
    # list, splitting the two tokens divided by the ambiguous space. Then, other
    # ambiguities are resolved separately for each token. Finally, the cartesian
    # product of tokens is computed, one time merging them with a space and one time
    # merging them without, to account for possible real and fake spaces.

    alternatives = [[uncertain_word]]
    while any(map(lambda alt: has_uncertainty(alt, uncertainty_chars),
                  itertools.chain(*alternatives))):
        for t, alternatives_token in enumerate(alternatives):
            for alt in alternatives_token:
                if has_uncertainty(alt, uncertainty_chars):
                    alternatives[t].remove(alt)

                    if uncertainty_chars['UNCERTAIN_SPACE'] in alt:
                        tokens = alt.split(uncertainty_chars['UNCERTAIN_SPACE'], 1)
                        alternatives[-1] = [tokens[0]]
                        alternatives += [[tokens[1]]]
                        continue

                    if len(re.findall(ALT_READING, alt)) > 0:
                        alternatives[t].append(re.sub(ALT_READING, '\g<1>', alt, 1))
                        alternatives[t].append(re.sub(ALT_READING, '\g<2>', alt, 1))
                        continue

                    if uncertainty_chars['MULTIPLE_UNCERTAINTY'] in alt:
                        alternatives[t] += list(map(lambda x: alt.replace(
                            uncertainty_chars['MULTIPLE_UNCERTAINTY'], x, 1),
                            multiple_uncertainty_replacements))
                        continue

                    if uncertainty_chars['SINGLE_UNCERTAINTY'] in alt:
                        alternatives[t] += list(map(lambda x: alt.replace(
                            uncertainty_chars['SINGLE_UNCERTAINTY'], x, 1),
                            alphabet))

    # Compute cartesian product
    current = alternatives[0]
    remaining = alternatives[1:]
    while(len(remaining) > 0):
        new_current = []
        for curr in current:
            for rem in remaining[0]:
                new_current.append(curr + rem)
                new_current.append(curr + ' ' + rem)
        current = new_current
        remaining = remaining[1:]

    alternatives_joined = current
    return alternatives_joined


def contextualize_sentence(sentence, corrupted_sentence, uncertainty_chars,
                           alphabet, convert_uncertainties=None, is_voynich=False):
    '''
    Generate a list of Uncertainty from a corrupted sentence, eventually
    converting all uncertainties to a single character.

    Args:
        uncertain_word (str): input uncertain word
        uncertainty_chars (dict): mapping of (type of uncertainty, char representation)
        alphabet (list): known non-space characters in the alphabet
        convert_uncertainties (str): if not None, convert all uncertainties to a single
            specified character and keep them in the training sentence and in context
        is_voynich (bool): True if sentence is from the Voynich manuscript
            (hence, already corrupted), False otherwise

    Returns:
        (list of Uncertainty): list Uncertainty belonging to the corrupted sentence.
        (str): corrupted sentence.
        (str): corrupted cleaned sentence, removing or converting uncertainties
    '''

    uncertainties = list(uncertainty_chars.values())
    char_to_uncertainty = {value: key for key, value in uncertainty_chars.items()}
    words = corrupted_sentence.split(' ')

    # Remove all words with uncertainties or convert uncertainties
    # to convert_uncertainties, if is not None
    if(convert_uncertainties is None):
        corrupted_clean_sentence = ' '.join([word for word in words if not
                                             has_uncertainty(word, uncertainty_chars)])

    else:
        corrupted_clean_sentence = re.sub(ALT_READING, convert_uncertainties, corrupted_sentence)
        corrupted_clean_sentence = re.sub('['+''.join(uncertainties)+']', convert_uncertainties, corrupted_clean_sentence)

    uncertainties_list = []

    for (index, word) in enumerate(words):
        if(not has_uncertainty(word, uncertainty_chars)):
            continue
        if(convert_uncertainties is None):
            left_context = list(filter(lambda x: not has_uncertainty(x, uncertainty_chars), words[:index]))
            right_context = list(filter(lambda x: not has_uncertainty(x, uncertainty_chars), words[index+1:]))
        else:
            left_context = corrupted_clean_sentence.split(' ')[:index]
            right_context = corrupted_clean_sentence.split(' ')[index+1:]
        alternatives_list = create_alternatives(word, uncertainty_chars, alphabet)
        alternatives_list = list(set(alternatives_list))

        if(is_voynich):
            correct_word = ''
        else:
            # Match corrupted token with original sentence
            left_chars = re.split('['+''.join(uncertainties)+'\[\]]',
                                  ' '.join(words[:index])[::-1])[0][::-1]
            left_chars += ' ' if len(left_chars) > 0 else ''
            right_chars = re.split('[\[\]'+''.join(uncertainties)+']',
                                   ' '.join(words[index+1:]))[0]
            right_chars = ' ' * (len(right_chars)>0) + right_chars

            correct_word = list(filter(lambda x: left_chars + x + right_chars
                                       in sentence, alternatives_list))
            if(len(correct_word) != 1):
                print(sentence)
                print(corrupted_sentence)
                print(word)
                print(alternatives_list)
                print(correct_word)
                raise RuntimeError('Implementation problems!')
            correct_word = correct_word[0]

        uncertainty_types = [char_to_uncertainty[unc] for unc in
                             uncertainties if unc in word]
        if len(re.findall(ALT_READING, word)) > 0:
            uncertainty_types.append('ALTERNATE_READING')

        uncertainties_list.append(Uncertainty(corrupted_word=word,
                                  correct_word=correct_word,
                                  left_context=left_context,
                                  right_context=right_context,
                                  alternatives_list=alternatives_list,
                                  uncertainty_types=uncertainty_types))

    return uncertainties_list, corrupted_clean_sentence
