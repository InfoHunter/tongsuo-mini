# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://github.com/Tongsuo-Project/tongsuo-mini/blob/main/LICENSE

import tf
import pytest


@pytest.mark.parametrize(
    "kat_file",
    [
        "test_sm3_data/sm3.txt",
    ],
)
def test_sm3(kat_file, subtests):
    with open(kat_file) as f:
        tb = {}

        for line in f:
            if line.startswith("#"):
                continue
            line = line.strip()
            if not line:
                continue

            name, value = line.partition("=")[::2]
            tb[name.strip()] = value.strip()

            if "Count" in tb and "Input" in tb and "Output" in tb:
                with subtests.test(i=tb["Count"]):
                    tf.ok(
                        "test_sm3 -input {} -output {}".format(
                            tb["Input"], tb["Output"]
                        )
                    )

                tb.clear()
