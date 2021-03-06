<?php

/**
 * @file
 * Miscellaneous helper functions.
 */

/**
 * Converts given model descriptions into Drupal form select element #options.
 *
 * @see islandora_get_content_models()
 *
 * @param array $content_models
 *   A list of content models as returned by islandora_get_content_models().
 *
 * @return array
 *   An associative array of content models for use in Drupal form select
 *   element options:
 *   - pid: The PID of the content model
 *     - label: The label to display to the user.
 */
function islandora_basic_collection_get_content_models_as_form_options(array $content_models) {
  $map_content_models_to_options = function($o) {
    $label = isset($o['label']) ? $o['label'] : $o['name'];
    return format_string('!pid ~ !label', array(
      '!pid' => $o['pid'],
      '!label' => $label,
    ));
  };
  return array_map($map_content_models_to_options, $content_models);
}

/**
 * Lists collections for use in a Drupal form select element #options.
 *
 * Ignores the given collection.
 *
 * @see islandora_get_collections()
 *
 * @param AbstractObject $object
 *   A collection object to ignore.
 *
 * @return array
 *   An associative array of collections for use in Drupal form select
 *   element options:
 *   - pid: The PID of the collection object.
 *     - label: The label to display to the user.
 */
function islandora_basic_collection_get_other_collections_as_form_options(AbstractObject $object) {
  $map_options = function($o) {
    return filter_xss($o['label']);
  };
  $options = array_map($map_options, islandora_basic_collection_get_collections());
  // Ignore given collection.
  unset($options[$object->id]);
  return $options;
}

/**
 * Gets a choose namespace element for use in the collection management forms.
 *
 * @param string $default_value
 *   The default value to use for the namespace field.
 *
 * @return array
 *   A Drupal form element for selecting the a namespace.
 */
function islandora_basic_collection_get_namespace_form_element($default_value) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $restrict_namespace = variable_get('islandora_namespace_restriction_enforced', FALSE);
  return array(
    '#type' => $restrict_namespace ? 'select' : 'textfield',
    '#title' => 'Children namespace',
    '#description' => t('Namespace for objects with selected content models.'),
    '#size' => 15,
    '#options' => $restrict_namespace ? drupal_map_assoc(islandora_get_allowed_namespaces()) : NULL,
    '#default_value' => $default_value,
  );
}

/**
 * A Drupal form select table populated with the given collection's children.
 *
 * @param AbstractObject $object
 *   The collection object.
 * @param array $pager_options
 *   An associative array containing options to pass onto the pager.
 *   - element: The numeric ID of the pager
 *   - fragment: The url fragment to append onto pager links, includes '#'.
 *
 * @return array
 *   A Drupal select table form element.
 */
function islandora_basic_collection_get_children_select_table_form_element(AbstractObject $object, array $pager_options) {
  // Assumes all results are returned although the function description
  // states otherwise.
  $limit = 10;
  $page = pager_find_page($pager_options['element']);
  list($count, $results) = islandora_basic_collection_get_member_objects($object, $page, $limit, 'manage');
  $page = pager_default_initialize($count, $limit, $pager_options['element']);
  $rows = array();
  foreach ($results as $result) {
    $pid = $result['object']['value'];
    $label = empty($result['title']['value']) ? t('Untitled') : filter_xss($result['title']['value']);
    $rows[$pid] = array(
      'label' => array(
        'data' => array(
          '#type' => 'link',
          '#title' => $label,
          '#href' => "islandora/object/{$pid}")));
  }
  // Theme pager doesn't support url fragments in D7 so we insert manually.
  $pager = theme('pager', array('quantity' => 10, 'element' => $pager_options['element']));
  $pattern = '/href="([^"]+)"/';
  $replace = 'href="\1' . $pager_options['fragment'] . '"';
  $pager = preg_replace($pattern, $replace, $pager);
  return array(
    '#type' => 'tableselect',
    '#header' => array('label' => array('data' => t('Label'))),
    '#options' => $rows,
    '#empty' => t('Collection is empty.'),
    '#prefix' => $pager,
    '#suffix' => $pager,
  );
}

/**
 * Gets a list of all immediate child PIDs.
 *
 * @param AbstractObject $object
 *   The collection object.
 * @param string $type
 *   Limit the child PID's to those of the given content model.
 *
 * @return array
 *   The list of child PIDs.
 */
function islandora_basic_collection_get_child_pids(AbstractObject $object, $type = NULL) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $message = islandora_deprecated('7.x-1.2', 'Code should be made to page over all objects to avoid potential memory issues with large collections.');
  trigger_error(filter_xss($message), E_USER_DEPRECATED);

  $params = array(
    'object' => $object,
    'page_size' => -1,
  );

  if (isset($type)) {
    $params['model'] = "<info:fedora/$type>";
  }

  $map_results = function($o) {
    return $o['object']['value'];
  };

  $query_info = islandora_basic_collection_get_query_info($params);
  $results = $object->repository->ri->query($query_info['query'], $query_info['type']);

  return array_map($map_results, $results);
}

/**
 * Gets a list of all parent pids.
 *
 * @param AbstractObject $object
 *   The object.
 *
 * @return array
 *   The list of parent PIDs.
 */
function islandora_basic_collection_get_parent_pids(AbstractObject $object) {
  $results = array_merge(
      $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection'),
      $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf'));
  $map_results = function($o) {
    return $o['object']['value'];
  };
  $collections = array_map($map_results, $results);
  return array_unique(array_filter($collections));
}

/**
 * Gets a list of all parent pids excluding the given parent.
 *
 * @param AbstractObject $object
 *   The object.
 * @param AbstractObject $parent
 *   The parent of the object.
 *
 * @return array
 *   The list of parent PIDs.
 */
function islandora_basic_collection_get_other_parents(AbstractObject $object, AbstractObject $parent) {
  $parents = islandora_basic_collection_get_parent_pids($object);
  unset($parents[array_search($parent->id, $parents)]);
  return $parents;
}

/**
 * Adds the given object to the given collection.
 *
 * @param AbstractObject $new_member
 *   The object to add.
 * @param AbstractObject $collection
 *   The collection object to add to.
 */
function islandora_basic_collection_add_to_collection(AbstractObject $new_member, AbstractObject $collection) {
  $results = $new_member->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection->id);
  if (empty($results)) {
    $new_member->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection->id);
  }
}

/**
 * Removes the given object from the given collection.
 *
 * @param AbstractObject $member
 *   The object to remove.
 * @param AbstractObject $collection
 *   The collection object to remove from.
 */
function islandora_basic_collection_remove_from_collection(AbstractObject $member, AbstractObject $collection) {
  $member->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $collection->id);
  $member->relationships->remove(FEDORA_RELS_EXT_URI, 'isMemberOf', $collection->id);
}
