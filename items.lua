-- items deffile
objects = {}

objects['fruit'] = {
    description = {
        short = "A delicious fruit.",
        long = [[The sweet and fleshy product of a tree or other plant that
contains seed and can be eaten as food]],
    }
}

objects['apple'] = {
    inherits = {'fruit'},
}

objects['orange'] = {
    inherits = {'fruit'},
}

objects['pear'] = {
    inherits = {'fruit'},
}

objects['avocado'] = {
    inherits = {'pear'},
}

objects['hass avocado'] = {
    inherits = {'avocado', 'fruit'},
}

objects['multicado'] = {
    inherits = {'avocado', 'fruit'}
}

objects['para-orange'] = {
    inherits = {'multicado', 'fruit', 'orange'}
}

