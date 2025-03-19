import React, { useState } from 'react';
import { Box, Flex, Text, Button, VStack, HStack } from '@chakra-ui/react';
import { DeleteIcon, ViewIcon, ViewOffIcon } from '@chakra-ui/icons';

const RouteBox = ({ route, onDelete, onShowRoute }) => {
  const startLabel = route.start_city ? route.start_city : route.start_coords;
  const endLabel = route.end_city ? route.end_city : route.end_coords;

  const formatTime = (timeStr) => {
    const date = new Date(timeStr);
    return date.toLocaleTimeString([], {
      hour: "2-digit",
      minute: "2-digit",
      hour12: false,
    });
  };

  const [routeVisible, setRouteVisible] = useState(false);
  const toggleRouteVisibility = () => {
    const newState = !routeVisible;
    setRouteVisible(newState);
    if (onShowRoute) {
      onShowRoute(route.id, newState);
    }
  };

  return (
    <Box borderWidth="1px" borderRadius="md" p={2} mb={0}>
      <Flex align="center" justifyContent="space-between">
        <Button
          size="sm"
          variant="outline"
          flex={1}
          height="60px"
          p={0}
          display="flex"
          flexDirection="column"
          alignItems="center"
          justifyContent="center"
          onClick={(e) => {
            e.stopPropagation();
            onDelete(route.id);
          }}
        >
          <DeleteIcon boxSize="5" mb="1" />
          <Text fontSize="xs">
            Delete
            <br />
            route
          </Text>
        </Button>
        <VStack spacing={1} flex={1} mx={4} align="center">
          <HStack spacing={2}>
            <Text fontSize="sm" fontWeight="bold">
              {startLabel}
            </Text>
            <Text fontSize="sm" color="gray.600">
              {formatTime(route.start_time)}
            </Text>
          </HStack>
          <Text fontSize="sm" color="gray.500">
            {route.total_distance} km
          </Text>
          <HStack spacing={2}>
            <Text fontSize="sm" fontWeight="bold">
              {endLabel}
            </Text>
            <Text fontSize="sm" color="gray.600">
              {route.end_time ? formatTime(route.end_time) : "-"}
            </Text>
          </HStack>
        </VStack>
        <Button
          size="sm"
          variant="outline"
          flex={1}
          height="60px"
          p={0}
          display="flex"
          flexDirection="column"
          alignItems="center"
          justifyContent="center"
          onClick={(e) => {
            e.stopPropagation();
            toggleRouteVisibility();
          }}
        >
          {routeVisible ? <ViewOffIcon boxSize="5" mb="1" /> : <ViewIcon boxSize="5" mb="1" />}
          {routeVisible ? (
            <Text fontSize="xs">Hide<br />route</Text>
          ) : (
            <Text fontSize="xs">Show<br />route</Text>
          )}
        </Button>
      </Flex>
    </Box>
  );
};

export default RouteBox;
