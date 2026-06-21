import os

import yaml

from thirdparty.errors import RecipeException
from thirdparty._internal.util.files import load, save


def update_recipe_data(recipe, data):
    """
    Tool to modify the ``recipe_data.yml`` once it is exported. It can be used, for example:

       - To add additional data like the "commit" and "url" for the scm.
       - To modify the contents cleaning the data that belong to other versions (different
         from the exported) to avoid changing the recipe revision when the changed data doesn't
         belong to the current version.

    :param recipe: The current recipe object. Always use ``self``.
    :param data: (Required) A dictionary (can be nested), of values to update
    """

    if not hasattr(recipe, "export_folder") or recipe.export_folder is None:
        raise RecipeException("The 'update_recipe_data()' can only be used in the 'export()' method")
    path = os.path.join(recipe.export_folder, "recipe_data.yml")
    if os.path.exists(path):
        recipe_data = load(path)
        recipe_data = yaml.safe_load(recipe_data)
    else:  # File doesn't exist, create it
        recipe_data = {}

    def recursive_dict_update(d, u):
        for k, v in u.items():
            if isinstance(v, dict):
                d[k] = recursive_dict_update(d.get(k, {}), v)
            else:
                d[k] = v
        return d

    recursive_dict_update(recipe_data, data)
    new_content = yaml.safe_dump(recipe_data)
    save(path, new_content)


def trim_recipe_data(recipe, raise_if_missing=True):
    """
    Tool to modify the ``recipe_data.yml`` once it is exported, to limit it to the current version
    only
    """
    if not hasattr(recipe, "export_folder") or recipe.export_folder is None:
        raise RecipeException("The 'trim_recipe_data()' tool can only be used in "
                             "the 'export()' method or 'post_export()' hook")
    path = os.path.join(recipe.export_folder, "recipe_data.yml")
    if not os.path.exists(path):
        if raise_if_missing:
            raise RecipeException("recipe_data.yml file doesn't exist")
        else:
            recipe.output.warning("recipe_data.yml file doesn't exist")
            return

    recipe_data = load(path)
    recipe_data = yaml.safe_load(recipe_data)

    version = str(recipe.version)
    result = {}
    for k, v in recipe_data.items():
        if k == "scm" or not isinstance(v, dict):
            result[k] = v
            continue  # to allow user extra recipe_data, common to all versions
        version_data = v.get(version)
        if version_data is not None:
            result[k] = {version: version_data}

    # Update the internal recipe data too
    recipe.recipe_data = result

    new_recipe_data_yml = yaml.safe_dump(result, default_flow_style=False)
    save(path, new_recipe_data_yml)

