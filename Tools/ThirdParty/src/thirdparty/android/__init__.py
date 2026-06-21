from thirdparty._internal.model.cpp_info import CppInfo as _CppInfo


def CppInfo(recipe):
    # Creation of a CppInfo object, to decouple the creation from the actual internal location
    # that at the moment doesn't require a ``recipe`` argument, but might require in the future
    # and allow us to refactor the location of recipe_model.build_info import CppInfo
    return _CppInfo()

