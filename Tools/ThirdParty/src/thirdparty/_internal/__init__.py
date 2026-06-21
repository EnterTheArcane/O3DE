from thirdparty.errors import RecipeException


REVISIONS = "revisions"  # capability


def check_duplicated_generator(generator, recipe):
    if generator.__class__.__name__ in recipe.generators:
        raise RecipeException(f"{generator.__class__.__name__} is declared in the generators "
                             "attribute, but was instantiated in the generate() method too. "
                             "It should only be present in one of them.")



