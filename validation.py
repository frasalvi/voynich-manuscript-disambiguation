import numpy as np
import numpy.linalg as npl
import pandas as pd


def predict_uncertain_word(model, context_left, context_right,
                           alternatives_list, how='cosine'):
    '''
    Predict an uncertain word given the context, ranking a set
    of possible alternatives according to cosine similarity or
    softmax probabilities.

    Args:
        model (Word2Vec): embeddings trained model.
        context_left(list of str): context at the left of the uncertain word
        context_right(list of str): context at the right of the uncertain word
        alternatives_list (list of str): list of alternatives for replacing the uncertainty
        how ('cosine' or 'softmax'): compute similarity using cosine distance
            or using softmax with the trained weights.

    Returns:
        (list of (str, float)): similarities between alternatives and the missing word.
    '''

    context_global = context_left + context_right

    # If no context is given, assign 0 similarity to all the alternatives
    if(len(context_global) == 0):
        alternatives_similarity = [(alternative, 0) for alternative in alternatives_list]
        return alternatives_list

    context_vectors_global = [model.wv[word] for word in context_global]
    l1_global = np.sum(context_vectors_global, axis=0)
    l1_global /= npl.norm(l1_global, 2)

    # Compute similarity for one single alternative
    def compute_similarity(alternative):
        alternative_words = alternative.split(' ')
        l1s = []

        if(len(alternative_words) == 1):
            l1 = l1_global
            l1s.append(l1)
        else:
            for i, alt_word in enumerate(alternative_words):
                cont_left = context_left[i:] + alternative_words[:i]
                cont_right = (alternative_words[i+1:] or []) + \
                    context_right[:len(context_right)-len(alternative_words)+i+1]
                cont = cont_left + cont_right
                context_vectors = [model.wv[word] for word in cont]
                l1 = np.sum(context_vectors, axis=0)
                l1 /= npl.norm(l1, 2)
                l1s.append(l1)

        similarities = []
        for i, l1 in enumerate(l1s):
            if(how == 'cosine'):
                similarities.append(np.array(model.wv[alternative_words[i]] @ l1)
                                    / npl.norm(model.wv[alternative_words[i]], 2))

            elif(how == 'softmax'):
                try:
                    similarities.append(np.exp(np.dot(
                        l1, model.syn1neg[model.wv.get_index(alternative_words[i])].T)))
                except KeyError as e:
                    print(e)
                    print('Some alternative words are not in the vocabulary, try with \'cosine\' option')

            else:
                raise RuntimeError('Unknown option')

        return np.array(similarities).mean()


    alternatives_similarity = [(alternative, compute_similarity(alternative))
                               for alternative in alternatives_list]

    alternatives_similarity = sorted(alternatives_similarity,
                                     key=lambda x: x[1], reverse=True)

    return alternatives_similarity


def predict_corruption(model, window, uncertainties_list, how='cosine'):
    '''
    Apply the model, predicting artificially corrupted words given the context.

    Args:
        model (Word2Vec): embeddings trained model.
        window (int): window size
        uncertainties_list (list of Uncertainty): list of uncertainties in the text
        how ('cosine' or 'softmax'): compute similarity using cosine distance
            or using softmax with the trained weights.

    Returns:
        (list of str): correct outputs
        (list of str): predicted outputs
        (list of str): type of corruption
    '''

    results = list(map(lambda uncertainty: (
        uncertainty.correct_word,
        predict_uncertain_word(model, uncertainty.left_context[-window:],
                               uncertainty.right_context[:window],
                               uncertainty.alternatives_list, how)[0][0],
        uncertainty.uncertainty_types),
                       uncertainties_list))

    y, predictions, types = list(zip(*results))
    return y, predictions, types


def predict_uncertainty(model, window, uncertainties_list, how='cosine'):
    '''
    Apply the model, predicting uncertain words given the context.

    Args:
        model (Word2Vec): embeddings trained model.
        window (int): window size
        uncertainties_list (list of Uncertainty): list of uncertainties in the text
        how ('cosine' or 'softmax'): compute similarity using cosine distance
            or using softmax with the trained weights.

    Returns:
        (list of (str, list of (str, float))): list of uncertain words and
            similarities for all their possible alternatives
    '''

    results = list(map(lambda uncertainty: (
        uncertainty.corrupted_word,
        predict_uncertain_word(model, uncertainty.left_context[-window:],
                               uncertainty.right_context[:window],
                               uncertainty.alternatives_list, how)),
                       uncertainties_list))

    return results


def evaluate_model(y, predictions, types):
    '''
    Evaluate the model, computing accuracies over different types of uncertainties.

    Args:
        y (list of str): correct outputs
        predictions(list of str): predicted outputs
        types (list of str): type of corruption

    Returns:
        (list of (str, float): accuracy over the different types of uncertainties.
    '''
    results = pd.DataFrame({'y': y, 'prediction': predictions,
                            'types': types}).set_index(['y', 'prediction'])
    results = results.types.apply(pd.Series).stack().reset_index().rename({0: 'type'}, axis=1)
    accuracies = results.groupby('type').apply(lambda x: x.y == x.prediction).groupby('type')
    accuracies = accuracies.sum() / accuracies.size()
    accuracies['overall'] = (np.array(y) == np.array(predictions)).sum() / len(y)

    return accuracies


def predict_and_evaluate_model(model, window, uncertainties_list, how='cosine'):
    '''
    Apply and evaluate the model, predicting artificially corrupted
    words given the context and then computing accuracies over
    different types of uncertainties.

    Args:
        model (Word2Vec): embeddings trained model.
        window (int): window size
        uncertainties_list (list of Uncertainty): list of uncertainties in the text
        how ('cosine' or 'softmax'): compute similarity using cosine distance
            or using softmax with the trained weights.

    Returns:
        (list of (str, float): accuracy over the different types of uncertainties.
    '''
    y, predictions, types = predict_corruption(model, window, uncertainties_list, how)
    accuracies = evaluate_model(y, predictions, types)
    return accuracies
