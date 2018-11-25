#snowflake

An implementation of twitter's snowflake with C , build in php extension.

### Install
```
$ /path/to/phpize
$ ./configure --with-php-config=/path/to/php-config
$ make && make install
```

### Ini
```
extension = snowflake.so

```

### Functions

```
snowflake_id(int $work_node = 0)

$work_node: An uniq ID should between 0 and 1023 

```

### Test

```php
<?php
echo snowflake_id(123);

```

