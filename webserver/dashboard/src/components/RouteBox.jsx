import React from "react";
import {
  Box,
  Flex,
  Text,
  Button,
  VStack,
  HStack,
  useDisclosure,
  Modal,
  ModalOverlay,
  ModalContent,
  ModalHeader,
  ModalCloseButton,
  ModalBody,
  ModalFooter,
  useToast,
} from "@chakra-ui/react";
import { DeleteIcon, ViewIcon, ViewOffIcon } from "@chakra-ui/icons";

const formatTime = (timeStr) => {
  const date = new Date(timeStr);
  const day = String(date.getDate()).padStart(2, "0");
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const hours = String(date.getHours()).padStart(2, "0");
  const minutes = String(date.getMinutes()).padStart(2, "0");
  return `${day}.${month} ${hours}:${minutes}`;
};

const formatCoordinates = (lat, lon) => {
  const latStr = `${Math.abs(lat).toFixed(4)}° ${lat >= 0 ? "N" : "S"}`;
  const lonStr = `${Math.abs(lon).toFixed(4)}° ${lon >= 0 ? "E" : "W"}`;
  return `${latStr}, ${lonStr}`;
};

const RouteBox = ({ route, onDeleteRoute, onShowRoute }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  const toast = useToast();
  let startLocation = route.start_city;
  if (!startLocation && route.start_coords) {
    const parts = route.start_coords.split(" ");
    if (parts.length === 2) {
      const lat = parseFloat(parts[0]);
      const lon = parseFloat(parts[1]);
      startLocation = formatCoordinates(lat, lon);
    }
  }

  let endLocation = route.end_city;
  if (!endLocation && route.end_coords) {
    const parts = route.end_coords.split(" ");
    if (parts.length === 2) {
      const lat = parseFloat(parts[0]);
      const lon = parseFloat(parts[1]);
      endLocation = formatCoordinates(lat, lon);
    }
  }

  const distanceKm = (route.total_distance / 1000).toFixed(2);

  const isVisible = route.isVisible;

  return (
    <Box borderWidth="1px" borderRadius="md" p={2} textAlign="center">
      <VStack spacing={1} align="stretch">
        <Box>
          <Text fontSize="sm" fontWeight="bold">
            {startLocation || "N/A"}
          </Text>
        </Box>
        <Flex justifyContent="space-between" alignItems="center" gap={2}>
          <Box flex="1" display="flex" justifyContent="center">
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
                onOpen();
              }}>
              <DeleteIcon boxSize="4" mb="1" />
              <Text fontSize="xs" textAlign="center">
                Delete
                <br />
                route
              </Text>
            </Button>
          </Box>
          <Box flex="1" textAlign="center">
            <VStack spacing={1}>
              <Text fontSize="sm">{formatTime(route.start_time)}</Text>
              <Text fontSize="sm">{distanceKm} km</Text>
              <Text fontSize="sm">
                {route.end_time ? formatTime(route.end_time) : "-"}
              </Text>
            </VStack>
          </Box>
          <Box flex="1" display="flex" justifyContent="center">
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
                onShowRoute(route.id, !isVisible);
              }}>
              {isVisible ? (
                <ViewOffIcon boxSize="4" mb="1" />
              ) : (
                <ViewIcon boxSize="4" mb="1" />
              )}
              <Text fontSize="xs" textAlign="center">
                {isVisible ? "Hide" : "Show"}
                <br />
                route
              </Text>
            </Button>
          </Box>
        </Flex>
        <Box>
          <Text fontSize="sm" fontWeight="bold">
            {endLocation || "N/A"}
          </Text>
        </Box>
      </VStack>

      <Modal isOpen={isOpen} onClose={onClose} isCentered>
        <ModalOverlay />
        <ModalContent>
          <ModalHeader>Delete Route</ModalHeader>
          <ModalCloseButton />
          <ModalBody>
            <Text>
              Are you sure you want to delete route with ID: {route.id} ?<br />
              <br />
              Route start location: {startLocation || "N/A"}
              <br />
              Route start time: {formatTime(route.start_time)}
              <br />
              Route end location: {endLocation || "N/A"}
              <br />
              Route end time: {route.end_time ? formatTime(route.end_time) : "-"}
              <br />
              Route distance: {distanceKm} km
              <br />
            </Text>
          </ModalBody>
          <ModalFooter>
            <Button
              colorScheme="red"
              mr={3}
              onClick={async () => {
                try {
                  const token = localStorage.getItem("authToken");
                  const response = await fetch(
                    `https://webapi.vehiclemap.xyz/routes/${route.id}`,
                    {
                      method: "DELETE",
                      headers: {
                        Authorization: `Bearer ${token}`,
                      },
                    }
                  );
                  if (!response.ok) {
                    throw new Error(`Failed with status ${response.status}`);
                  }
                  toast({
                    title: "Route deleted",
                    description: "Route has been removed.",
                    status: "success",
                    duration: 3000,
                    isClosable: true,
                  });
                  onClose();
                  await onShowRoute(route.id, false);
                  onDeleteRoute(route.id);
                  if (typeof window.refreshVehicles === "function") {
                    window.refreshVehicles();
                  }
                } catch (error) {
                  console.error("Error deleting route:", error);
                  toast({
                    title: "Error",
                    description: "Could not delete route.",
                    status: "error",
                    duration: 5000,
                    isClosable: true,
                  });
                }
              }}>
              Delete
            </Button>
            <Button variant="ghost" onClick={onClose}>
              Cancel
            </Button>
          </ModalFooter>
        </ModalContent>
      </Modal>
    </Box>
  );
};

export default RouteBox;
